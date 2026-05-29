/**
 * @file ESP32Connectivity.cpp
 * @brief Implementação do gerenciador não-bloqueante de WiFi e MQTT.
 *
 * @author  professorThiago (https://github.com/professorThiago)
 * @version 3.0.0
 * @license MIT
 */

#include "ESP32Connectivity.h"
#include "DebugManager.h"

ESP32Connectivity  conectividade;
ESP32Connectivity* ESP32Connectivity::_instancia = nullptr;

// =============================================================================
// Tradução de códigos de erro
// =============================================================================

static const char* traduzirStatusWiFi(int s)
{
    switch (s) {
        case 255: return "255: NO_SHIELD — hardware WiFi nao encontrado";
        case 0:   return "0: IDLE — WiFi ocioso";
        case 1:   return "1: NO_SSID_AVAIL — rede nao encontrada (verifique o SSID)";
        case 2:   return "2: SCAN_COMPLETED — varredura concluida";
        case 3:   return "3: CONNECTED";
        case 4:   return "4: CONNECT_FAILED — falha (verifique a senha)";
        case 5:   return "5: CONNECTION_LOST — sinal fraco ou AP reiniciou";
        case 6:   return "6: DISCONNECTED — desconectado";
        default:  return "DESCONHECIDO";
    }
}

static const char* traduzirCodigoMQTT(int c)
{
    switch (c) {
        case -4: return "-4: TIMEOUT — broker nao respondeu no keepalive";
        case -3: return "-3: CONNECTION_LOST — conexao TCP caiu";
        case -2: return "-2: CONNECT_FAILED — falha TCP/TLS (verifique host e porta)";
        case -1: return "-1: DISCONNECTED — desconectado normalmente";
        case  0: return "0: CONNECTED";
        case  1: return "1: BAD_PROTOCOL — versao do protocolo rejeitada";
        case  2: return "2: BAD_CLIENT_ID — client ID rejeitado";
        case  3: return "3: UNAVAILABLE — broker indisponivel";
        case  4: return "4: BAD_CREDENTIALS — usuario ou senha incorretos";
        case  5: return "5: UNAUTHORIZED — nao autorizado (ACL/certificado)";
        default: return "DESCONHECIDO";
    }
}

// =============================================================================
// Inicialização — três sobrecargas (SIMPLES, TLS, AWS)
// =============================================================================

void ESP32Connectivity::beginSimples(const ConfigWiFi& wifi,
                                      const ConfigMQTT& mqtt,
                                      const ConfigTopicos& topicos)
{
    _modo    = ModoConexao::SIMPLES;
    _wifi    = wifi;
    _mqtt    = mqtt;
    _topicos = topicos;
    _iniciarWiFi();
}

void ESP32Connectivity::beginTLS(const ConfigWiFi& wifi,
                                  const ConfigMQTT& mqtt,
                                  const ConfigTLS& tls,
                                  const ConfigTopicos& topicos)
{
    _modo    = ModoConexao::TLS;
    _wifi    = wifi;
    _mqtt    = mqtt;
    _tls     = tls;
    _topicos = topicos;
    _iniciarWiFi();
}

void ESP32Connectivity::beginAWS(const ConfigWiFi& wifi,
                                   const ConfigAWS& aws,
                                   const ConfigTopicos& topicos)
{
    _modo    = ModoConexao::AWS_IOT;
    _wifi    = wifi;
    _aws     = aws;
    _topicos = topicos;
    _iniciarWiFi();
}

void ESP32Connectivity::_iniciarWiFi()
{
    _instancia = this;

    // Zera a fila
    for (uint8_t i = 0; i < CONNECTIVITY_FILA_SLOTS; i++) _fila[i].ocupado = false;
    _filaInicio = _filaFim = _filaTamanho = 0;

    _configurarClienteMQTT();

    debugInfo("==============================");
    debugInfo(" ESP32Connectivity v3.0.0");
    debugInfo("==============================");
    debugInfo("Iniciando conexao WiFi...");
    debugVerbose("SSID: " + String(_wifi.ssid));

    WiFi.mode(WIFI_STA);
    WiFi.begin(_wifi.ssid, _wifi.senha);

    _estadoWiFi        = EstadoWiFi::CONECTANDO;
    _inicioConexaoWiFi = millis();
}

// =============================================================================
// Configuração do buffer MQTT
// =============================================================================

void ESP32Connectivity::configurarBufferMQTT(uint16_t tamanhoBytes)
{
    _mqttClient.setBufferSize(tamanhoBytes);
    debugVerbose("Buffer MQTT configurado: " + String(tamanhoBytes) + " bytes");
}

// =============================================================================
// Loop principal
// =============================================================================

void ESP32Connectivity::update()
{
    _processarWiFi();
    _processarMQTT();

    if (_estadoMQTT == EstadoMQTT::CONECTADO)
    {
        _mqttClient.loop();
        _drenaFila();
    }
}

// =============================================================================
// Máquina de estados — WiFi
// =============================================================================

void ESP32Connectivity::_processarWiFi()
{
    bool conectadoAgora = (WiFi.status() == WL_CONNECTED);

    switch (_estadoWiFi)
    {
        case EstadoWiFi::CONECTANDO:
        {
            if (conectadoAgora)
            {
                _estadoWiFi = EstadoWiFi::CONECTADO;
                debugInfo("WiFi conectado!");
                debugInfo("IP: " + WiFi.localIP().toString());
                debugVerbose("RSSI: " + String(WiFi.RSSI()) + " dBm");

                if (_cbWiFiConectado) _cbWiFiConectado();

                _estadoMQTT          = EstadoMQTT::DESCONECTADO;
                _tentativasMQTT      = 0;
                _ultimaTentativaMQTT = 0;
            }
            else if (millis() - _inicioConexaoWiFi > WIFI_TIMEOUT_MS)
            {
                int ws = WiFi.status();
                debugAviso("WiFi: timeout (" + String(WIFI_TIMEOUT_MS / 1000) + "s).");
                debugErro("Status: " + String(traduzirStatusWiFi(ws)));

                if (ws == 1) debugAviso("Dica: SSID '" + String(_wifi.ssid) + "' nao encontrado.");
                if (ws == 4) debugAviso("Dica: verifique a senha do WiFi.");

                _estadoWiFi        = EstadoWiFi::DESCONECTADO;
                _inicioConexaoWiFi = millis();
            }
            else
            {
                static uint32_t ultimoPonto = 0;
                if (millis() - ultimoPonto >= 2000) {
                    ultimoPonto = millis();
                    debugVerbose("WiFi: aguardando... (" +
                                 String((millis() - _inicioConexaoWiFi) / 1000) + "s)");
                }
            }
            break;
        }

        case EstadoWiFi::CONECTADO:
        {
            if (!conectadoAgora)
            {
                int ws = WiFi.status();
                _estadoWiFi = EstadoWiFi::DESCONECTADO;
                debugErro("WiFi desconectado!");
                debugErro("Status: " + String(traduzirStatusWiFi(ws)));

                if (ws == 5) debugAviso("Dica: sinal fraco ou roteador reiniciou.");
                if (ws == 1) debugAviso("Dica: rede sumiu do alcance.");

                if (_cbWiFiDesconectado) _cbWiFiDesconectado();

                // MQTT cai junto
                if (_estadoMQTT == EstadoMQTT::CONECTADO) {
                    _estadoMQTT = EstadoMQTT::DESCONECTADO;
                    debugAviso("MQTT desconectado junto com o WiFi.");
                    if (_cbMQTTDesconectado) _cbMQTTDesconectado();
                }

                _inicioConexaoWiFi = millis();
            }
            break;
        }

        case EstadoWiFi::DESCONECTADO:
        {
            if (millis() - _inicioConexaoWiFi >= WIFI_RETRY_INTERVALO)
            {
                debugAviso("WiFi: tentando reconectar...");
                debugVerbose("Status atual: " + String(traduzirStatusWiFi(WiFi.status())));
                WiFi.disconnect();
                WiFi.begin(_wifi.ssid, _wifi.senha);
                _estadoWiFi        = EstadoWiFi::CONECTANDO;
                _inicioConexaoWiFi = millis();
            }
            break;
        }
    }
}

// =============================================================================
// Máquina de estados — MQTT
// =============================================================================

void ESP32Connectivity::_processarMQTT()
{
    if (_estadoWiFi != EstadoWiFi::CONECTADO) return;

    switch (_estadoMQTT)
    {
        case EstadoMQTT::DESCONECTADO:
        {
            if (millis() - _ultimaTentativaMQTT < MQTT_RETRY_INTERVALO) return;

            if (_tentativasMQTT >= MQTT_MAX_TENTATIVAS)
            {
                uint32_t pausa = MQTT_RETRY_INTERVALO * 6;
                debugAviso("MQTT: maximo de tentativas. Pausando " + String(pausa / 1000) + "s...");
                _tentativasMQTT      = 0;
                _ultimaTentativaMQTT = millis() + (MQTT_RETRY_INTERVALO * 5);
                return;
            }

            debugVerbose("MQTT: tentativa " + String(_tentativasMQTT + 1) +
                         "/" + String(MQTT_MAX_TENTATIVAS));
            _ultimaTentativaMQTT = millis();
            _tentativasMQTT++;
            _estadoMQTT = EstadoMQTT::CONECTANDO;
            break;
        }

        case EstadoMQTT::CONECTANDO:
        {
            if (_tentarConectarMQTT())
            {
                _estadoMQTT     = EstadoMQTT::CONECTADO;
                _tentativasMQTT = 0;
                debugInfo("MQTT conectado!");

                const char* cid = (_modo == ModoConexao::AWS_IOT) ? _aws.clientId : _mqtt.clientId;
                debugVerbose("Client ID: " + String(cid));

                _inscreverTopicos();
                if (_topicos.nPublicar > 0)
                    _publicarDireto(_topicos.publicar[0], "ESP32 conectado ao MQTT");

                if (_cbMQTTConectado) _cbMQTTConectado();
            }
            else
            {
                int codigo = _mqttClient.state();
                debugErro("MQTT: falha na conexao!");
                debugErro("Codigo: " + String(traduzirCodigoMQTT(codigo)));

                if (codigo == 4) debugAviso("Dica: verifique usuario e senha do broker.");
                if (codigo == 2) debugAviso("Dica: tente outro clientId.");
                if (codigo == -2) debugAviso("Dica: verifique broker, porta e certificados.");
                if (codigo == 5) debugAviso("Dica: verifique certificados ou permissoes ACL.");

                _estadoMQTT = EstadoMQTT::DESCONECTADO;
            }
            break;
        }

        case EstadoMQTT::CONECTADO:
        {
            if (!_mqttClient.connected())
            {
                int codigo = _mqttClient.state();
                _estadoMQTT = EstadoMQTT::DESCONECTADO;
                debugErro("MQTT desconectado inesperadamente!");
                debugErro("Codigo: " + String(traduzirCodigoMQTT(codigo)));
                debugAviso("Tentando reconectar...");
                if (_cbMQTTDesconectado) _cbMQTTDesconectado();
            }
            break;
        }
    }
}

bool ESP32Connectivity::_tentarConectarMQTT()
{
    if (_modo == ModoConexao::AWS_IOT)
    {
        debugVerbose("MQTT: conectando como AWS IoT (mTLS)...");
        return _mqttClient.connect(_aws.clientId);
    }

    if (strlen(_mqtt.usuario) > 0)
    {
        debugVerbose("MQTT: conectando com usuario/senha...");
        return _mqttClient.connect(_mqtt.clientId, _mqtt.usuario, _mqtt.senha);
    }

    debugVerbose("MQTT: conectando anonimamente...");
    return _mqttClient.connect(_mqtt.clientId);
}

void ESP32Connectivity::_inscreverTopicos()
{
    debugVerbose("MQTT: inscrevendo em " + String(_topicos.nReceber) + " topico(s)...");
    for (int i = 0; i < _topicos.nReceber; i++)
    {
        const char* t = _topicos.receber[i];
        if (!t || strlen(t) == 0) continue;
        if (_mqttClient.subscribe(t))
            debugInfo("MQTT: inscrito em '" + String(t) + "'");
        else
            debugErro("MQTT: falha ao inscrever em '" + String(t) + "'");
    }
}

// =============================================================================
// Configuração do cliente MQTT
// =============================================================================

void ESP32Connectivity::_configurarClienteMQTT()
{
    debugInfo("Modo de conexao: " +
              String(_modo == ModoConexao::SIMPLES ? "MQTT SIMPLES" :
                     _modo == ModoConexao::TLS     ? "MQTT TLS"     : "AWS IOT CORE"));

    if (_modo == ModoConexao::AWS_IOT)
    {
        debugVerbose("Endpoint: " + String(_aws.endpoint));
        debugVerbose("Porta: "    + String(_aws.porta));
        _wifiClienteSeguro.setCACert(_aws.certCA);
        _wifiClienteSeguro.setCertificate(_aws.certCRT);
        _wifiClienteSeguro.setPrivateKey(_aws.certPrivate);
        _mqttClient.setClient(_wifiClienteSeguro);
        _mqttClient.setServer(_aws.endpoint, _aws.porta);
    }
    else if (_modo == ModoConexao::TLS)
    {
        debugVerbose("Broker: " + String(_mqtt.broker));
        debugVerbose("Porta: "  + String(_mqtt.porta));

        if (_tls.certificadoCA && strlen(_tls.certificadoCA) > 100)
        {
            debugVerbose("Certificado CA: configurado.");
            _wifiClienteSeguro.setCACert(_tls.certificadoCA);
        }
        else
        {
            debugAviso("Certificado CA ausente — usando setInsecure() (APENAS PARA TESTES!)");
            _wifiClienteSeguro.setInsecure();
        }

        _mqttClient.setClient(_wifiClienteSeguro);
        _mqttClient.setServer(_mqtt.broker, _mqtt.porta);
    }
    else // SIMPLES
    {
        debugVerbose("Broker: " + String(_mqtt.broker));
        debugVerbose("Porta: "  + String(_mqtt.porta));
        _mqttClient.setClient(_wifiCliente);
        _mqttClient.setServer(_mqtt.broker, _mqtt.porta);
    }

    _mqttClient.setCallback(_callbackMQTTEstatico);
}

// =============================================================================
// Publicação — FIX: enfileira sempre que não conseguir publicar direto
// =============================================================================

bool ESP32Connectivity::publicar(int indiceTopico, const char* mensagem)
{
    return publicar(topicoPublicacao(indiceTopico), mensagem);
}

bool ESP32Connectivity::publicar(const char* topico, const char* mensagem)
{
    if (!topico || strlen(topico) == 0)
    {
        debugErro("publicar(): topico vazio ou invalido.");
        return false;
    }

    // Estratégia: enfileira SEMPRE primeiro, depois tenta enviar.
    // Se o envio direto funcionar, remove da fila.
    // Garante que nenhuma mensagem seja perdida mesmo se a conexão
    // cair entre a verificação e o envio efetivo.
    _enfileirar(topico, mensagem);

    // Tenta drenar a fila imediatamente se estiver conectado
    bool redeOk = (_estadoWiFi == EstadoWiFi::CONECTADO) &&
                  (WiFi.status() == WL_CONNECTED)         &&
                  (_estadoMQTT  == EstadoMQTT::CONECTADO) &&
                  (_mqttClient.connected());

    if (redeOk) _drenaFila();

    // Retorna true se a fila foi drenada completamente (mensagem enviada)
    return (_filaTamanho == 0);
}

bool ESP32Connectivity::_publicarDireto(const char* topico, const char* payload)
{
    bool ok = _mqttClient.publish(topico, payload);
    if (ok) {
        debugInfo("MQTT publicado em '" + String(topico) + "'");
        debugTudo("Payload: " + String(payload));
    } else {
        debugErro("MQTT: falha ao publicar em '" + String(topico) + "'");
        debugErro("Codigo: " + String(traduzirCodigoMQTT(_mqttClient.state())));
    }
    return ok;
}

// =============================================================================
// Fila offline
// =============================================================================

void ESP32Connectivity::_enfileirar(const char* topico, const char* payload)
{
    if (_filaTamanho >= CONNECTIVITY_FILA_SLOTS)
    {
        debugAviso("Fila cheia (" + String(CONNECTIVITY_FILA_SLOTS) +
                   " slots) — descartando mensagem mais antiga.");
        _filaInicio = (_filaInicio + 1) % CONNECTIVITY_FILA_SLOTS;
        _filaTamanho--;
    }

    MensagemFila& slot = _fila[_filaFim];
    strncpy(slot.topico,  topico,  CONNECTIVITY_FILA_TOPICO_MAX  - 1);
    strncpy(slot.payload, payload, CONNECTIVITY_FILA_PAYLOAD_MAX - 1);
    slot.topico [CONNECTIVITY_FILA_TOPICO_MAX  - 1] = '\0';
    slot.payload[CONNECTIVITY_FILA_PAYLOAD_MAX - 1] = '\0';
    slot.ocupado = true;

    _filaFim = (_filaFim + 1) % CONNECTIVITY_FILA_SLOTS;
    _filaTamanho++;
    debugVerbose("Mensagem enfileirada. Fila: " + String(_filaTamanho) +
                 "/" + String(CONNECTIVITY_FILA_SLOTS));
}

void ESP32Connectivity::_drenaFila()
{
    if (_filaTamanho == 0) return;

    debugInfo("Drenando fila: " + String(_filaTamanho) + " mensagem(ns)...");

    while (_filaTamanho > 0 && _mqttClient.connected())
    {
        MensagemFila& slot = _fila[_filaInicio];
        if (slot.ocupado)
        {
            if (!_publicarDireto(slot.topico, slot.payload)) break;
            slot.ocupado = false;
        }
        _filaInicio = (_filaInicio + 1) % CONNECTIVITY_FILA_SLOTS;
        _filaTamanho--;
    }

    if (_filaTamanho == 0)
        debugInfo("Fila drenada com sucesso.");
}

// =============================================================================
// Estado e tópicos
// =============================================================================

bool    ESP32Connectivity::wifiConectado()   const { return _estadoWiFi == EstadoWiFi::CONECTADO; }
bool    ESP32Connectivity::mqttConectado()   const { return _estadoMQTT == EstadoMQTT::CONECTADO; }
uint8_t ESP32Connectivity::mensagensNaFila() const { return _filaTamanho; }

const char* ESP32Connectivity::topicoPublicacao(int i) const
{
    if (i < 0 || i >= _topicos.nPublicar) {
        debugErro("topicoPublicacao(): indice invalido: " + String(i));
        return "";
    }
    return _topicos.publicar[i];
}

const char* ESP32Connectivity::topicoRecebimento(int i) const
{
    if (i < 0 || i >= _topicos.nReceber) {
        debugErro("topicoRecebimento(): indice invalido: " + String(i));
        return "";
    }
    return _topicos.receber[i];
}

// =============================================================================
// Callbacks
// =============================================================================

void ESP32Connectivity::registrarCallbackMensagem(CallbackMensagemMQTT cb)
{
    _cbMensagem = cb;
    debugVerbose(cb ? "Callback de mensagem registrado." : "Callback de mensagem removido.");
}

void ESP32Connectivity::registrarCallbackWiFiConectado   (CallbackWiFi cb) { _cbWiFiConectado    = cb; }
void ESP32Connectivity::registrarCallbackWiFiDesconectado(CallbackWiFi cb) { _cbWiFiDesconectado = cb; }
void ESP32Connectivity::registrarCallbackMQTTConectado   (CallbackMQTT cb) { _cbMQTTConectado    = cb; }
void ESP32Connectivity::registrarCallbackMQTTDesconectado(CallbackMQTT cb) { _cbMQTTDesconectado = cb; }

// =============================================================================
// Callback interno PubSubClient
// =============================================================================

void ESP32Connectivity::_callbackMQTTEstatico(char* topico, byte* payload, unsigned int tamanho)
{
    if (_instancia) _instancia->_callbackMQTT(topico, payload, tamanho);
}

void ESP32Connectivity::_callbackMQTT(char* topico, byte* payload, unsigned int tamanho)
{
    String mensagem;
    mensagem.reserve(tamanho);
    for (unsigned int i = 0; i < tamanho; i++) mensagem += (char)payload[i];

    debugInfo("Mensagem recebida em '" + String(topico) + "'");
    debugTudo("Payload (" + String(tamanho) + " bytes): " + mensagem);

    if (_cbMensagem)
        _cbMensagem(topico, mensagem);
    else
        debugAviso("Mensagem recebida mas nenhum callback registrado.");
}
