/**
 * @file ESP32Connectivity.cpp
 * @brief Implementação do gerenciador não-bloqueante de WiFi e MQTT.
 *
 * @author  professorThiago (https://github.com/professorThiago)
 * @version 2.0.0
 * @license MIT
 */

#include "ESP32Connectivity.h"
#include "secrets.h"
#include "DebugManager.h"

ESP32Connectivity conectividade;
ESP32Connectivity* ESP32Connectivity::_instancia = nullptr;

// =============================================================================
// Tradução dos códigos de estado do PubSubClient
// =============================================================================

/**
 * @brief Converte o código inteiro de estado do PubSubClient em texto legível.
 *
 * Códigos definidos em PubSubClient.h:
 *  -4  MQTT_CONNECTION_TIMEOUT     — broker não respondeu no keepalive
 *  -3  MQTT_CONNECTION_LOST        — conexão TCP caiu durante a sessão
 *  -2  MQTT_CONNECT_FAILED         — falha na camada TCP/TLS
 *  -1  MQTT_DISCONNECTED           — desconectado normalmente
 *   0  MQTT_CONNECTED              — conectado
 *   1  MQTT_CONNECT_BAD_PROTOCOL   — versão do protocolo rejeitada
 *   2  MQTT_CONNECT_BAD_CLIENT_ID  — client ID rejeitado
 *   3  MQTT_CONNECT_UNAVAILABLE    — broker indisponível
 *   4  MQTT_CONNECT_BAD_CREDENTIALS— usuário ou senha incorretos
 *   5  MQTT_CONNECT_UNAUTHORIZED   — não autorizado (ACL / certificado)
 */
static const char* traduzirCodigoMQTT(int codigo)
{
    switch (codigo)
    {
        case -4: return "-4: TIMEOUT — broker nao respondeu no keepalive";
        case -3: return "-3: CONNECTION_LOST — conexao TCP caiu";
        case -2: return "-2: CONNECT_FAILED — falha TCP/TLS (verifique host e porta)";
        case -1: return "-1: DISCONNECTED — desconectado normalmente";
        case  0: return "0: CONNECTED";
        case  1: return "1: BAD_PROTOCOL — versao do protocolo rejeitada pelo broker";
        case  2: return "2: BAD_CLIENT_ID — client ID rejeitado (tente outro ID)";
        case  3: return "3: UNAVAILABLE — broker indisponivel ou sobrecarregado";
        case  4: return "4: BAD_CREDENTIALS — usuario ou senha incorretos";
        case  5: return "5: UNAUTHORIZED — nao autorizado (verifique ACL/certificado)";
        default: return "DESCONHECIDO";
    }
}

/**
 * @brief Converte o código de status WiFi (wl_status_t) em texto legível.
 *
 * Códigos definidos em WiFi.h (wl_status_t):
 *  255  WL_NO_SHIELD       — hardware WiFi não encontrado
 *    0  WL_IDLE_STATUS     — WiFi ocioso entre comandos
 *    1  WL_NO_SSID_AVAIL   — rede (SSID) não encontrada no alcance
 *    2  WL_SCAN_COMPLETED  — varredura de redes concluída
 *    3  WL_CONNECTED       — conectado
 *    4  WL_CONNECT_FAILED  — falha ao conectar (senha incorreta?)
 *    5  WL_CONNECTION_LOST — conexão perdida (sinal fraco ou AP reiniciou)
 *    6  WL_DISCONNECTED    — desconectado (tentando ou comando manual)
 */
static const char* traduzirStatusWiFi(int status)
{
    switch (status)
    {
        case 255: return "255: NO_SHIELD — hardware WiFi nao encontrado";
        case 0:   return "0: IDLE — WiFi ocioso";
        case 1:   return "1: NO_SSID_AVAIL — rede nao encontrada (verifique o SSID)";
        case 2:   return "2: SCAN_COMPLETED — varredura concluida";
        case 3:   return "3: CONNECTED — conectado";
        case 4:   return "4: CONNECT_FAILED — falha (verifique a senha)";
        case 5:   return "5: CONNECTION_LOST — sinal fraco ou AP reiniciou";
        case 6:   return "6: DISCONNECTED — desconectado";
        default:  return "DESCONHECIDO";
    }
}

// =============================================================================
// Inicialização
// =============================================================================

void ESP32Connectivity::begin()
{
    _instancia = this;

    for (uint8_t i = 0; i < CONNECTIVITY_FILA_TAMANHO; i++)
        _fila[i].ocupado = false;
    _filaInicio  = 0;
    _filaFim     = 0;
    _filaTamanho = 0;

    debugInfo("==============================");
    debugInfo(" ESP32Connectivity v2.0.0");
    debugInfo("==============================");

    _configurarClienteMQTT();

    debugInfo("Iniciando conexao WiFi...");
    debugVerbose("SSID: " + String(WIFI_SSID));

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_SENHA);

    _estadoWiFi        = EstadoWiFi::CONECTANDO;
    _inicioConexaoWiFi = millis();
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
                debugVerbose("Canal: " + String(WiFi.channel()));

                if (_cbWiFiConectado) _cbWiFiConectado();

                _estadoMQTT          = EstadoMQTT::DESCONECTADO;
                _tentativasMQTT      = 0;
                _ultimaTentativaMQTT = 0;
            }
            else if (millis() - _inicioConexaoWiFi > WIFI_TIMEOUT_MS)
            {
                debugAviso("WiFi: timeout (" + String(WIFI_TIMEOUT_MS / 1000) +
                           "s). Tentando novamente em " +
                           String(WIFI_RETRY_INTERVALO / 1000) + "s...");
                debugErro("Status: " + String(traduzirStatusWiFi(WiFi.status())));

                int wStatus = WiFi.status();
                if (wStatus == 1)
                    debugAviso("Dica: verifique se o SSID '" + String(WIFI_SSID) + "' esta correto e no alcance.");
                else if (wStatus == 4)
                    debugAviso("Dica: verifique a senha do WiFi em secrets.cpp.");

                _estadoWiFi        = EstadoWiFi::DESCONECTADO;
                _inicioConexaoWiFi = millis();
            }
            else
            {
                // Ponto de progresso a cada 2s enquanto conectando
                static uint32_t ultimoPonto = 0;
                if (millis() - ultimoPonto >= 2000)
                {
                    ultimoPonto = millis();
                    debugVerbose("WiFi: aguardando conexao... (" +
                                 String((millis() - _inicioConexaoWiFi) / 1000) + "s)");
                }
            }
            break;
        }

        case EstadoWiFi::CONECTADO:
        {
            if (!conectadoAgora)
            {
                _estadoWiFi = EstadoWiFi::DESCONECTADO;
                debugErro("WiFi desconectado!");
                debugErro("Status: " + String(traduzirStatusWiFi(WiFi.status())));

                int wStatus = WiFi.status();
                if (wStatus == 5)
                    debugAviso("Dica: sinal fraco ou roteador reiniciou. Reconectando...");
                else if (wStatus == 1)
                    debugAviso("Dica: rede sumiu do alcance. Verifique o roteador.");

                if (_cbWiFiDesconectado) _cbWiFiDesconectado();

                if (_estadoMQTT == EstadoMQTT::CONECTADO)
                {
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
                WiFi.begin(WIFI_SSID, WIFI_SENHA);
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
                uint32_t pausaMs = MQTT_RETRY_INTERVALO * 6;
                debugAviso("MQTT: maximo de tentativas atingido (" +
                           String(MQTT_MAX_TENTATIVAS) + "). Pausando " +
                           String(pausaMs / 1000) + "s...");
                _tentativasMQTT      = 0;
                _ultimaTentativaMQTT = millis() + (MQTT_RETRY_INTERVALO * 5);
                return;
            }

            debugVerbose("MQTT: iniciando tentativa " +
                         String(_tentativasMQTT + 1) + "/" +
                         String(MQTT_MAX_TENTATIVAS) + "...");

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

                debugInfo("MQTT conectado ao broker!");
                debugVerbose("Client ID: " + String(USAR_AWS_IOT ? AWS_IOT_CLIENT_ID : MQTT_CLIENT_ID));

                _inscreverTopicos();
                _publicarDireto(topicoPublicacao(0), "ESP32 conectado ao MQTT");

                if (_cbMQTTConectado) _cbMQTTConectado();
            }
            else
            {
                int codigo = _mqttClient.state();
                debugErro("MQTT: falha na conexao!");
                debugErro("Codigo: " + String(traduzirCodigoMQTT(codigo)));

                // Diagnóstico adicional por código
                if (codigo == 4)
                    debugAviso("Dica: verifique MQTT_USUARIO e MQTT_SENHA em secrets.cpp");
                else if (codigo == 2)
                    debugAviso("Dica: tente mudar MQTT_CLIENT_ID em secrets.cpp");
                else if (codigo == -2)
                    debugAviso("Dica: verifique MQTT_BROKER, MQTT_PORTA e certificados");
                else if (codigo == 5)
                    debugAviso("Dica: verifique certificados ou permissoes no broker");

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
    if (USAR_AWS_IOT)
    {
        debugVerbose("MQTT: conectando como AWS IoT (mTLS)...");
        return _mqttClient.connect(AWS_IOT_CLIENT_ID);
    }

    if (strlen(MQTT_USUARIO) > 0)
    {
        debugVerbose("MQTT: conectando com usuario/senha...");
        return _mqttClient.connect(MQTT_CLIENT_ID, MQTT_USUARIO, MQTT_SENHA);
    }

    debugVerbose("MQTT: conectando anonimamente...");
    return _mqttClient.connect(MQTT_CLIENT_ID);
}

void ESP32Connectivity::_inscreverTopicos()
{
    debugVerbose("MQTT: inscrevendo em " + String(TOTAL_TOPICOS_RECEBER) + " topico(s)...");

    for (int i = 0; i < TOTAL_TOPICOS_RECEBER; i++)
    {
        const char* topico = topicoRecebimento(i);
        if (strlen(topico) == 0) continue;

        if (_mqttClient.subscribe(topico))
            debugInfo("MQTT: inscrito em '" + String(topico) + "'");
        else
            debugErro("MQTT: falha ao inscrever em '" + String(topico) + "'");
    }
}

// =============================================================================
// Configuração do cliente MQTT
// =============================================================================

void ESP32Connectivity::_configurarClienteMQTT()
{
    debugInfo("Configurando cliente MQTT...");

    if (USAR_AWS_IOT)
    {
        debugInfo("Modo: AWS IoT Core (mTLS)");
        debugVerbose("Endpoint: " + String(AWS_IOT_ENDPOINT));
        debugVerbose("Porta: " + String(AWS_IOT_PORT));
        debugVerbose("Client ID: " + String(AWS_IOT_CLIENT_ID));

        _wifiClienteSeguro.setCACert(AWS_CERT_CA);
        _wifiClienteSeguro.setCertificate(AWS_CERT_CRT);
        _wifiClienteSeguro.setPrivateKey(AWS_CERT_PRIVATE);
        _mqttClient.setClient(_wifiClienteSeguro);
        _mqttClient.setServer(AWS_IOT_ENDPOINT, AWS_IOT_PORT);
    }
    else if (MQTT_TLS)
    {
        debugInfo("Modo: MQTT com TLS");
        debugVerbose("Broker: " + String(MQTT_BROKER));
        debugVerbose("Porta: "  + String(MQTT_PORTA));

        if (strlen(MQTT_CERTIFICADO_CA) > 100)
        {
            debugVerbose("Certificado CA: configurado.");
            _wifiClienteSeguro.setCACert(MQTT_CERTIFICADO_CA);
        }
        else
        {
            debugAviso("Certificado CA ausente — usando setInsecure() (APENAS PARA TESTES!)");
            _wifiClienteSeguro.setInsecure();
        }

        _mqttClient.setClient(_wifiClienteSeguro);
        _mqttClient.setServer(MQTT_BROKER, MQTT_PORTA);
    }
    else
    {
        debugInfo("Modo: MQTT sem TLS");
        debugVerbose("Broker: " + String(MQTT_BROKER));
        debugVerbose("Porta: "  + String(MQTT_PORTA));

        _mqttClient.setClient(_wifiCliente);
        _mqttClient.setServer(MQTT_BROKER, MQTT_PORTA);
    }

    _mqttClient.setCallback(_callbackMQTTEstatico);
    debugVerbose("Callback interno MQTT registrado.");
}

// =============================================================================
// Publicação e fila offline
// =============================================================================

bool ESP32Connectivity::publicar(int indiceTopico, const char* mensagem)
{
    return publicar(topicoPublicacao(indiceTopico), mensagem);
}

bool ESP32Connectivity::publicar(const char* topico, const char* mensagem)
{
    if (strlen(topico) == 0)
    {
        debugErro("publicar(): topico vazio.");
        return false;
    }

    if (_estadoMQTT == EstadoMQTT::CONECTADO && _mqttClient.connected())
        return _publicarDireto(topico, mensagem);

    debugAviso("MQTT offline — mensagem enfileirada.");
    debugVerbose("Topico: " + String(topico));
    debugVerbose("Fila: " + String(_filaTamanho + 1) + "/" + String(CONNECTIVITY_FILA_TAMANHO));
    _enfileirar(topico, mensagem);
    return false;
}

bool ESP32Connectivity::_publicarDireto(const char* topico, const char* payload)
{
    bool ok = _mqttClient.publish(topico, payload);

    if (ok)
    {
        debugInfo("MQTT publicado em '" + String(topico) + "'");
        debugTudo("Payload: " + String(payload));
    }
    else
    {
        debugErro("MQTT: falha ao publicar em '" + String(topico) + "'");
        debugErro("Codigo: " + String(traduzirCodigoMQTT(_mqttClient.state())));
    }

    return ok;
}

void ESP32Connectivity::_enfileirar(const char* topico, const char* payload)
{
    if (_filaTamanho >= CONNECTIVITY_FILA_TAMANHO)
    {
        debugAviso("Fila offline cheia (" + String(CONNECTIVITY_FILA_TAMANHO) +
                   ") — descartando mensagem mais antiga.");
        _filaInicio = (_filaInicio + 1) % CONNECTIVITY_FILA_TAMANHO;
        _filaTamanho--;
    }

    MensagemFila& slot = _fila[_filaFim];
    strncpy(slot.topico,  topico,  CONNECTIVITY_FILA_TOPICO_MAX  - 1);
    strncpy(slot.payload, payload, CONNECTIVITY_FILA_PAYLOAD_MAX - 1);
    slot.topico [CONNECTIVITY_FILA_TOPICO_MAX  - 1] = '\0';
    slot.payload[CONNECTIVITY_FILA_PAYLOAD_MAX - 1] = '\0';
    slot.ocupado = true;

    _filaFim = (_filaFim + 1) % CONNECTIVITY_FILA_TAMANHO;
    _filaTamanho++;

    debugVerbose("Mensagem enfileirada. Fila atual: " + String(_filaTamanho));
}

void ESP32Connectivity::_drenaFila()
{
    if (_filaTamanho == 0) return;

    debugInfo("Drenando fila offline: " + String(_filaTamanho) + " mensagem(ns)...");

    while (_filaTamanho > 0 && _mqttClient.connected())
    {
        MensagemFila& slot = _fila[_filaInicio];
        if (slot.ocupado)
        {
            if (!_publicarDireto(slot.topico, slot.payload))
            {
                debugAviso("Drenagem interrompida — falha ao publicar. Tentativa no proximo update().");
                break;
            }
            slot.ocupado = false;
        }
        _filaInicio = (_filaInicio + 1) % CONNECTIVITY_FILA_TAMANHO;
        _filaTamanho--;
    }

    if (_filaTamanho == 0)
        debugInfo("Fila offline drenada com sucesso.");
}

// =============================================================================
// Estado
// =============================================================================

bool    ESP32Connectivity::wifiConectado()    const { return _estadoWiFi  == EstadoWiFi::CONECTADO;  }
bool    ESP32Connectivity::mqttConectado()    const { return _estadoMQTT  == EstadoMQTT::CONECTADO;  }
uint8_t ESP32Connectivity::mensagensNaFila()  const { return _filaTamanho; }

// =============================================================================
// Tópicos
// =============================================================================

const char* ESP32Connectivity::topicoPublicacao(int indice) const
{
    if (indice < 0 || indice >= TOTAL_TOPICOS_PUBLICAR)
    {
        debugErro("topicoPublicacao(): indice invalido: " + String(indice));
        return "";
    }
    return TOPICOS_PUBLICAR[indice];
}

const char* ESP32Connectivity::topicoRecebimento(int indice) const
{
    if (indice < 0 || indice >= TOTAL_TOPICOS_RECEBER)
    {
        debugErro("topicoRecebimento(): indice invalido: " + String(indice));
        return "";
    }
    return TOPICOS_RECEBER[indice];
}

// =============================================================================
// Callbacks
// =============================================================================

void ESP32Connectivity::registrarCallbackMensagem(CallbackMensagemMQTT cb)
{
    _cbMensagem = cb;
    debugVerbose(cb ? "Callback de mensagem registrado." : "Callback de mensagem removido.");
}

void ESP32Connectivity::registrarCallbackWiFiConectado(CallbackWiFi cb)     { _cbWiFiConectado        = cb; }
void ESP32Connectivity::registrarCallbackWiFiDesconectado(CallbackWiFi cb)  { _cbWiFiDesconectado     = cb; }
void ESP32Connectivity::registrarCallbackMQTTConectado(CallbackMQTT cb)     { _cbMQTTConectado        = cb; }
void ESP32Connectivity::registrarCallbackMQTTDesconectado(CallbackMQTT cb)  { _cbMQTTDesconectado     = cb; }

// =============================================================================
// Callback interno do PubSubClient
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
    debugTudo("Payload bruto (" + String(tamanho) + " bytes): " + mensagem);

    if (_cbMensagem)
        _cbMensagem(topico, mensagem);
    else
        debugAviso("Mensagem recebida mas nenhum callback registrado.");
}
