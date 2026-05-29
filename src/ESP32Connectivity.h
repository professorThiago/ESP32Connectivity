/**
 * @file ESP32Connectivity.h
 * @brief Gerenciamento não-bloqueante de WiFi e MQTT para ESP32.
 *
 * @details
 * Biblioteca independente — não depende de `secrets.h` do projeto.
 * Toda a configuração é passada via structs em `begin()`.
 *
 * @par Modos de conexão MQTT
 * | Modo | Quando usar |
 * |------|-------------|
 * | `ModoConexao::SIMPLES`  | Broker local sem criptografia |
 * | `ModoConexao::TLS`      | Broker público com TLS (HiveMQ, Mosquitto, etc.) |
 * | `ModoConexao::AWS_IOT`  | AWS IoT Core com mTLS (certificado de dispositivo) |
 *
 * @par Uso básico
 * @code
 * #include <ESP32Connectivity.h>
 *
 * // Preencha as structs com suas credenciais
 * ConfigWiFi wifi   = { "MinhaRede", "minha_senha" };
 * ConfigMQTT mqtt   = { "broker.exemplo.com", 8883, "esp32_id", "user", "pass" };
 * ConfigTLS  tls    = { meuCertificadoCA };
 * const char* pub[] = { "meu/topico/status" };
 * const char* rec[] = { "meu/topico/comando" };
 * ConfigTopicos topicos = { pub, 1, rec, 1 };
 *
 * void setup() {
 *     configurarDebug(DEBUG_INFO, 4);
 *     conectividade.begin(wifi, mqtt, ModoConexao::TLS, tls, topicos);
 * }
 *
 * void loop() {
 *     conectividade.update();
 *     conectividade.publicar(0, "hello");
 * }
 * @endcode
 *
 * @author  professorThiago (https://github.com/professorThiago)
 * @version 3.0.0
 * @license MIT
 */

#ifndef ESP32_CONNECTIVITY_H
#define ESP32_CONNECTIVITY_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// ---------------------------------------------------------------------------
// Configuração da fila offline
// Redefina ANTES do #include para personalizar sem alterar a biblioteca.
// ---------------------------------------------------------------------------

/** @brief Slots na fila offline (número de mensagens). */
#ifndef CONNECTIVITY_FILA_SLOTS
  #define CONNECTIVITY_FILA_SLOTS 10
#endif

/** @brief Tamanho máximo do tópico em cada slot (bytes). */
#ifndef CONNECTIVITY_FILA_TOPICO_MAX
  #define CONNECTIVITY_FILA_TOPICO_MAX 64
#endif

/** @brief Tamanho máximo do payload em cada slot (bytes). */
#ifndef CONNECTIVITY_FILA_PAYLOAD_MAX
  #define CONNECTIVITY_FILA_PAYLOAD_MAX 256
#endif

// ---------------------------------------------------------------------------
// Modo de conexão MQTT
// ---------------------------------------------------------------------------

/**
 * @brief Define o protocolo e nível de segurança da conexão MQTT.
 */
enum class ModoConexao : uint8_t {
    SIMPLES,   ///< MQTT sem criptografia (porta 1883 — apenas redes locais confiáveis)
    TLS,       ///< MQTT com TLS/SSL (porta 8883 — brokers públicos)
    AWS_IOT    ///< AWS IoT Core com mTLS — requer certificado de dispositivo (porta 8883)
};

// ---------------------------------------------------------------------------
// Structs de configuração
// ---------------------------------------------------------------------------

/**
 * @brief Credenciais WiFi.
 */
struct ConfigWiFi {
    const char* ssid;   ///< Nome da rede (SSID)
    const char* senha;  ///< Senha da rede
};

/**
 * @brief Configuração do broker MQTT.
 * Usada nos modos `SIMPLES` e `TLS`.
 */
struct ConfigMQTT {
    const char* broker;    ///< Endereço do broker (hostname ou IP)
    int         porta;     ///< Porta (1883 sem TLS, 8883 com TLS)
    const char* clientId;  ///< ID único deste dispositivo
    const char* usuario;   ///< Usuário (deixe "" para conexão anônima)
    const char* senha;     ///< Senha  (deixe "" para conexão anônima)
};

/**
 * @brief Certificado CA para modo `TLS`.
 * O certificado CA do broker em formato PEM.
 * Deixe `certificadoCA = ""` para usar `setInsecure()` (apenas testes).
 */
struct ConfigTLS {
    const char* certificadoCA;  ///< Certificado CA do broker em PEM
};

/**
 * @brief Certificados e configuração do AWS IoT Core (modo `AWS_IOT`).
 */
struct ConfigAWS {
    const char* endpoint;    ///< Endpoint AWS (xxxx.iot.região.amazonaws.com)
    int         porta;       ///< Porta (normalmente 8883)
    const char* clientId;    ///< Thing Name ou ID do dispositivo
    const char* certCA;      ///< Certificado raiz Amazon (AmazonRootCA1.pem)
    const char* certCRT;     ///< Certificado do dispositivo (.crt)
    const char* certPrivate; ///< Chave privada do dispositivo (.key)
};

/**
 * @brief Tópicos MQTT para publicação e recebimento.
 */
struct ConfigTopicos {
    const char** publicar;   ///< Array de tópicos de publicação
    int          nPublicar;  ///< Número de tópicos de publicação
    const char** receber;    ///< Array de tópicos de recebimento (inscrição)
    int          nReceber;   ///< Número de tópicos de recebimento
};

// ---------------------------------------------------------------------------
// Tipos de callback
// ---------------------------------------------------------------------------

typedef void (*CallbackMensagemMQTT)(const char* topico, const String& mensagem);
typedef void (*CallbackWiFi)(void);
typedef void (*CallbackMQTT)(void);

// ---------------------------------------------------------------------------
// Classe principal
// ---------------------------------------------------------------------------

/**
 * @brief Gerencia WiFi + MQTT de forma não-bloqueante com fila offline.
 */
class ESP32Connectivity
{
public:
    ESP32Connectivity() = default;

    // -----------------------------------------------------------------------
    // Inicialização
    // -----------------------------------------------------------------------

    /**
     * @brief Inicia no modo SIMPLES — sem criptografia.
     * Indicado para redes locais confiáveis. Porta padrão: 1883.
     *
     * @par Exemplo
     * @code
     * conectividade.beginSimples(wifi, mqtt, topicos);
     * @endcode
     */
    void beginSimples(const ConfigWiFi& wifi,
                      const ConfigMQTT& mqtt,
                      const ConfigTopicos& topicos);

    /**
     * @brief Inicia no modo TLS — conexão criptografada.
     * Indicado para brokers públicos (HiveMQ, Mosquitto Cloud, etc.). Porta padrão: 8883.
     *
     * @par Exemplo
     * @code
     * conectividade.beginTLS(wifi, mqtt, tls, topicos);
     * @endcode
     */
    void beginTLS(const ConfigWiFi& wifi,
                  const ConfigMQTT& mqtt,
                  const ConfigTLS& tls,
                  const ConfigTopicos& topicos);

    /**
     * @brief Inicia no modo AWS IoT Core — mTLS com certificado de dispositivo.
     * Porta padrão: 8883.
     *
     * @par Exemplo
     * @code
     * conectividade.beginAWS(wifi, aws, topicos);
     * @endcode
     */
    void beginAWS(const ConfigWiFi& wifi,
                  const ConfigAWS& aws,
                  const ConfigTopicos& topicos);

    // -----------------------------------------------------------------------
    // Loop principal
    // -----------------------------------------------------------------------

    /**
     * @brief Avança as máquinas de estado e drena a fila offline.
     * **Deve ser chamado em todo `loop()`.**
     */
    void update();

    // -----------------------------------------------------------------------
    // Configuração da fila (chame antes do begin(), se necessário)
    // -----------------------------------------------------------------------

    /**
     * @brief Configura o tamanho do buffer interno do PubSubClient.
     *
     * Por padrão o PubSubClient limita mensagens a 256 bytes. Use este
     * método para aumentar o limite se publicar payloads maiores (ex: JSON
     * longos, leituras de sensores completas).
     *
     * @param tamanhoBytes  Tamanho máximo da mensagem MQTT em bytes.
     *                      Inclui cabeçalho + tópico + payload.
     *                      Padrão: 256. Recomendado máximo: 4096 no ESP32.
     *
     * @note Chame **antes** de `begin()`.
     *
     * @par Exemplo
     * @code
     * conectividade.configurarBufferMQTT(1024);   // mensagens até 1 KB
     * conectividade.begin(wifi, mqtt, topicos);
     * @endcode
     */
    void configurarBufferMQTT(uint16_t tamanhoBytes);

    // -----------------------------------------------------------------------
    // Estado
    // -----------------------------------------------------------------------

    /** @brief Retorna `true` se o WiFi está conectado. */
    bool wifiConectado() const;

    /** @brief Retorna `true` se o MQTT está conectado. */
    bool mqttConectado() const;

    /** @brief Retorna o número de mensagens na fila offline. */
    uint8_t mensagensNaFila() const;

    // -----------------------------------------------------------------------
    // Publicação
    // -----------------------------------------------------------------------

    /**
     * @brief Publica em um tópico pelo índice.
     * Se o MQTT estiver offline, enfileira para envio posterior.
     *
     * @param indiceTopico  Índice em `ConfigTopicos::publicar[]`.
     * @param mensagem      Payload a publicar.
     * @return `true` se publicado imediatamente; `false` se enfileirado.
     */
    bool publicar(int indiceTopico, const char* mensagem);

    /**
     * @brief Publica em um tópico pelo nome completo.
     * Se o MQTT estiver offline, enfileira para envio posterior.
     *
     * @param topico    Tópico MQTT completo.
     * @param mensagem  Payload a publicar.
     * @return `true` se publicado imediatamente; `false` se enfileirado.
     */
    bool publicar(const char* topico, const char* mensagem);

    // -----------------------------------------------------------------------
    // Consulta de tópicos
    // -----------------------------------------------------------------------

    /** @brief Retorna o tópico de publicação pelo índice, ou `""` se inválido. */
    const char* topicoPublicacao(int indice) const;

    /** @brief Retorna o tópico de recebimento pelo índice, ou `""` se inválido. */
    const char* topicoRecebimento(int indice) const;

    // -----------------------------------------------------------------------
    // Callbacks
    // -----------------------------------------------------------------------

    void registrarCallbackMensagem         (CallbackMensagemMQTT cb);
    void registrarCallbackWiFiConectado    (CallbackWiFi cb);
    void registrarCallbackWiFiDesconectado (CallbackWiFi cb);
    void registrarCallbackMQTTConectado    (CallbackMQTT cb);
    void registrarCallbackMQTTDesconectado (CallbackMQTT cb);

private:
    // -----------------------------------------------------------------------
    // Máquina de estados
    // -----------------------------------------------------------------------

    enum class EstadoWiFi : uint8_t { DESCONECTADO, CONECTANDO, CONECTADO };
    enum class EstadoMQTT : uint8_t { DESCONECTADO, CONECTANDO, CONECTADO };

    EstadoWiFi _estadoWiFi = EstadoWiFi::DESCONECTADO;
    EstadoMQTT _estadoMQTT = EstadoMQTT::DESCONECTADO;

    static constexpr uint32_t WIFI_TIMEOUT_MS       = 15000;
    static constexpr uint32_t WIFI_RETRY_INTERVALO  = 5000;
    static constexpr uint32_t MQTT_RETRY_INTERVALO  = 5000;
    static constexpr uint8_t  MQTT_MAX_TENTATIVAS   = 5;

    uint32_t _inicioConexaoWiFi  = 0;
    uint32_t _ultimaTentativaMQTT = 0;
    uint8_t  _tentativasMQTT     = 0;

    void _processarWiFi();
    void _processarMQTT();
    bool _tentarConectarMQTT();
    void _inscreverTopicos();

    // -----------------------------------------------------------------------
    // Configuração armazenada
    // -----------------------------------------------------------------------

    ModoConexao  _modo = ModoConexao::SIMPLES;
    ConfigWiFi   _wifi    = {};
    ConfigMQTT   _mqtt    = {};
    ConfigTLS    _tls     = {};
    ConfigAWS    _aws     = {};
    ConfigTopicos _topicos = {};

    void _iniciarWiFi();
    void _configurarClienteMQTT();

    // -----------------------------------------------------------------------
    // Clientes de rede
    // -----------------------------------------------------------------------

    WiFiClient        _wifiCliente;
    WiFiClientSecure  _wifiClienteSeguro;
    PubSubClient      _mqttClient;

    // -----------------------------------------------------------------------
    // Fila offline (FIFO circular, tamanho fixo em compile-time)
    // -----------------------------------------------------------------------

    struct MensagemFila {
        char topico [CONNECTIVITY_FILA_TOPICO_MAX];
        char payload[CONNECTIVITY_FILA_PAYLOAD_MAX];
        bool ocupado = false;
    };

    MensagemFila _fila[CONNECTIVITY_FILA_SLOTS];
    uint8_t      _filaInicio  = 0;
    uint8_t      _filaFim     = 0;
    uint8_t      _filaTamanho = 0;

    void _enfileirar(const char* topico, const char* payload);
    void _drenaFila();
    bool _publicarDireto(const char* topico, const char* payload);

    // -----------------------------------------------------------------------
    // Callbacks
    // -----------------------------------------------------------------------

    CallbackMensagemMQTT _cbMensagem             = nullptr;
    CallbackWiFi         _cbWiFiConectado        = nullptr;
    CallbackWiFi         _cbWiFiDesconectado     = nullptr;
    CallbackMQTT         _cbMQTTConectado        = nullptr;
    CallbackMQTT         _cbMQTTDesconectado     = nullptr;

    static ESP32Connectivity* _instancia;
    static void _callbackMQTTEstatico(char* topico, byte* payload, unsigned int tamanho);
    void        _callbackMQTT(char* topico, byte* payload, unsigned int tamanho);
};

extern ESP32Connectivity conectividade;

#endif // ESP32_CONNECTIVITY_H
