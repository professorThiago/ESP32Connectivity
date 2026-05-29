/**
 * @file ESP32Connectivity.h
 * @brief Gerenciamento não-bloqueante de WiFi e MQTT para ESP32.
 *
 * @details
 * Esta biblioteca unifica o gerenciamento de conectividade WiFi e MQTT
 * usando **máquinas de estado** em vez de `while()` bloqueantes. O
 * programa principal nunca fica preso aguardando conexão — o `update()`
 * avança os estados a cada chamada no `loop()`.
 *
 * @par Funcionalidades
 * - Conexão WiFi e MQTT não-bloqueantes via máquina de estado.
 * - Reconexão automática de WiFi e MQTT quando a conexão cai.
 * - **Fila de mensagens offline:** publicações feitas sem MQTT conectado
 *   são enfileiradas e enviadas automaticamente ao reconectar.
 * - Suporte a MQTT simples, MQTT com TLS e AWS IoT Core (mTLS).
 * - Callbacks configuráveis para eventos de conexão/desconexão e
 *   recebimento de mensagens.
 *
 * @par Máquina de estados WiFi
 * @code
 *   DESCONECTADO ──begin()──> CONECTANDO ──sucesso──> CONECTADO
 *        ^                        |                       |
 *        |                    timeout/falha               |
 *        └────────────────────────┴──────── queda ────────┘
 * @endcode
 *
 * @par Máquina de estados MQTT
 * @code
 *   DESCONECTADO ──WiFi OK──> CONECTANDO ──sucesso──> CONECTADO
 *        ^                        |                       |
 *        |                    timeout/falha               |
 *        └────────────────────────┴──────── queda ────────┘
 * @endcode
 *
 * @par Uso básico
 * @code
 * #include <ESP32Connectivity.h>
 *
 * void aoReceberMensagem(const char* topico, const String& msg) {
 *     Serial.println("Recebi: " + msg);
 * }
 *
 * void setup() {
 *     conectividade.begin();
 *     conectividade.registrarCallbackMensagem(aoReceberMensagem);
 * }
 *
 * void loop() {
 *     conectividade.update();   // sempre no loop() — não bloqueia
 *
 *     // Publica mesmo offline: a mensagem vai para a fila
 *     conectividade.publicar(0, "hello world");
 * }
 * @endcode
 *
 * @note  `update()` **deve** ser chamado a cada iteração do `loop()`.
 *        Sem isso as máquinas de estado não avançam e a fila não é drenada.
 *
 * @author  professorThiago (https://github.com/professorThiago)
 * @version 1.0.0
 * @date    2026
 * @license MIT
 *
 * @par Licença MIT
 * Copyright (c) 2026 professorThiago\n
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:\n
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.\n
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
 */

#ifndef ESP32_CONNECTIVITY_H
#define ESP32_CONNECTIVITY_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// ---------------------------------------------------------------------------
// Configurações da fila offline (ajuste conforme a RAM disponível)
// ---------------------------------------------------------------------------

/** @brief Número máximo de mensagens retidas na fila offline. */
#ifndef CONNECTIVITY_FILA_TAMANHO
  #define CONNECTIVITY_FILA_TAMANHO 10
#endif

/** @brief Tamanho máximo do tópico em cada slot da fila (bytes). */
#ifndef CONNECTIVITY_FILA_TOPICO_MAX
  #define CONNECTIVITY_FILA_TOPICO_MAX 64
#endif

/** @brief Tamanho máximo do payload em cada slot da fila (bytes). */
#ifndef CONNECTIVITY_FILA_PAYLOAD_MAX
  #define CONNECTIVITY_FILA_PAYLOAD_MAX 256
#endif

// ---------------------------------------------------------------------------
// Tipos de callback
// ---------------------------------------------------------------------------

/** @brief Callback chamado ao receber uma mensagem MQTT. */
typedef void (*CallbackMensagemMQTT)(const char* topico, const String& mensagem);

/** @brief Callback chamado quando o WiFi conecta ou reconecta. */
typedef void (*CallbackWiFi)(void);

/** @brief Callback chamado quando o MQTT conecta, reconecta ou desconecta. */
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
    // -----------------------------------------------------------------------
    // Construtor
    // -----------------------------------------------------------------------

    ESP32Connectivity() = default;

    // -----------------------------------------------------------------------
    // Inicialização
    // -----------------------------------------------------------------------

    /**
     * @brief Inicializa WiFi e MQTT e inicia a tentativa de conexão.
     *
     * Deve ser chamado uma vez no `setup()`. A conexão é iniciada
     * de forma **não-bloqueante** — o `setup()` retorna imediatamente
     * e o `update()` avança os estados no `loop()`.
     *
     * @par Exemplo
     * @code
     * void setup() {
     *     configurarDebug();
     *     conectividade.begin();
     * }
     * @endcode
     */
    void begin();

    // -----------------------------------------------------------------------
    // Loop principal — DEVE ser chamado em todo loop()
    // -----------------------------------------------------------------------

    /**
     * @brief Avança as máquinas de estado e drena a fila offline.
     *
     * **Deve ser chamado em todo `loop()`**, sem delays longos entre
     * chamadas. Responsável por:
     * - Avançar a reconexão WiFi quando necessário.
     * - Avançar a reconexão MQTT quando necessário.
     * - Processar mensagens MQTT recebidas (`mqttClient.loop()`).
     * - Drenar a fila de mensagens offline ao reconectar.
     */
    void update();

    // -----------------------------------------------------------------------
    // Estado da conexão
    // -----------------------------------------------------------------------

    /**
     * @brief Retorna `true` se o WiFi está conectado.
     */
    bool wifiConectado() const;

    /**
     * @brief Retorna `true` se o MQTT está conectado.
     */
    bool mqttConectado() const;

    /**
     * @brief Retorna o número de mensagens atualmente na fila offline.
     */
    uint8_t mensagensNaFila() const;

    // -----------------------------------------------------------------------
    // Publicação de mensagens
    // -----------------------------------------------------------------------

    /**
     * @brief Publica uma mensagem em um tópico pelo índice.
     *
     * Se o MQTT estiver conectado, publica imediatamente.
     * Se não estiver, **enfileira** a mensagem para envio posterior.
     * Se a fila estiver cheia, a mensagem mais antiga é descartada.
     *
     * @param indiceTopico  Índice em `TOPICOS_PUBLICAR[]` (de secrets.h).
     * @param mensagem      Payload a publicar.
     * @return `true` se publicado imediatamente; `false` se enfileirado.
     *
     * @par Exemplo
     * @code
     * conectividade.publicar(0, "ligado");    // tópico 0
     * conectividade.publicar(1, "log: ok");   // tópico 1
     * @endcode
     */
    bool publicar(int indiceTopico, const char* mensagem);

    /**
     * @brief Publica uma mensagem em um tópico pelo nome completo.
     *
     * Segue a mesma lógica de enfileiramento de `publicar(indice, msg)`.
     *
     * @param topico    String com o tópico MQTT completo.
     * @param mensagem  Payload a publicar.
     * @return `true` se publicado imediatamente; `false` se enfileirado.
     */
    bool publicar(const char* topico, const char* mensagem);

    // -----------------------------------------------------------------------
    // Consulta de tópicos
    // -----------------------------------------------------------------------

    /**
     * @brief Retorna o tópico de publicação pelo índice.
     * @param indice  Índice em `TOPICOS_PUBLICAR[]`.
     * @return Ponteiro para a string do tópico, ou `""` se inválido.
     */
    const char* topicoPublicacao(int indice) const;

    /**
     * @brief Retorna o tópico de recebimento pelo índice.
     * @param indice  Índice em `TOPICOS_RECEBER[]`.
     * @return Ponteiro para a string do tópico, ou `""` se inválido.
     */
    const char* topicoRecebimento(int indice) const;

    // -----------------------------------------------------------------------
    // Callbacks de eventos
    // -----------------------------------------------------------------------

    /**
     * @brief Registra callback para mensagens MQTT recebidas.
     *
     * @param cb  Função `void cb(const char* topico, const String& mensagem)`.
     *
     * @par Exemplo
     * @code
     * void aoReceber(const char* topico, const String& msg) {
     *     Serial.println("Tópico: " + String(topico));
     *     Serial.println("Payload: " + msg);
     * }
     * conectividade.registrarCallbackMensagem(aoReceber);
     * @endcode
     */
    void registrarCallbackMensagem(CallbackMensagemMQTT cb);

    /**
     * @brief Registra callback chamado quando o WiFi conecta.
     * @param cb  Função `void cb()`.
     */
    void registrarCallbackWiFiConectado(CallbackWiFi cb);

    /**
     * @brief Registra callback chamado quando o WiFi desconecta.
     * @param cb  Função `void cb()`.
     */
    void registrarCallbackWiFiDesconectado(CallbackWiFi cb);

    /**
     * @brief Registra callback chamado quando o MQTT conecta.
     * @param cb  Função `void cb()`.
     */
    void registrarCallbackMQTTConectado(CallbackMQTT cb);

    /**
     * @brief Registra callback chamado quando o MQTT desconecta.
     * @param cb  Função `void cb()`.
     */
    void registrarCallbackMQTTDesconectado(CallbackMQTT cb);

private:
    // -----------------------------------------------------------------------
    // Máquina de estados WiFi
    // -----------------------------------------------------------------------

    enum class EstadoWiFi : uint8_t {
        DESCONECTADO,
        CONECTANDO,
        CONECTADO
    };

    EstadoWiFi _estadoWiFi    = EstadoWiFi::DESCONECTADO;
    uint32_t   _inicioConexaoWiFi = 0;
    bool       _wifiConectadoAntes = false;

    static constexpr uint32_t WIFI_TIMEOUT_MS      = 15000; ///< Timeout para conectar
    static constexpr uint32_t WIFI_RETRY_INTERVALO = 5000;  ///< Intervalo entre tentativas

    void _processarWiFi();

    // -----------------------------------------------------------------------
    // Máquina de estados MQTT
    // -----------------------------------------------------------------------

    enum class EstadoMQTT : uint8_t {
        DESCONECTADO,
        CONECTANDO,
        CONECTADO
    };

    EstadoMQTT _estadoMQTT        = EstadoMQTT::DESCONECTADO;
    uint32_t   _inicioConexaoMQTT = 0;
    uint32_t   _ultimaTentativaMQTT = 0;
    uint8_t    _tentativasMQTT    = 0;
    bool       _mqttConectadoAntes = false;

    static constexpr uint32_t MQTT_RETRY_INTERVALO = 5000;  ///< Intervalo entre tentativas
    static constexpr uint8_t  MQTT_MAX_TENTATIVAS  = 5;     ///< Tentativas antes de desistir temporariamente

    void _processarMQTT();
    bool _tentarConectarMQTT();
    void _inscreverTopicos();

    // -----------------------------------------------------------------------
    // Clientes de rede
    // -----------------------------------------------------------------------

    WiFiClient        _wifiCliente;
    WiFiClientSecure  _wifiClienteSeguro;
    PubSubClient      _mqttClient;

    void _configurarClienteMQTT();

    // -----------------------------------------------------------------------
    // Fila de mensagens offline
    // -----------------------------------------------------------------------

    struct MensagemFila {
        char topico[CONNECTIVITY_FILA_TOPICO_MAX];
        char payload[CONNECTIVITY_FILA_PAYLOAD_MAX];
        bool ocupado;
    };

    MensagemFila _fila[CONNECTIVITY_FILA_TAMANHO];
    uint8_t      _filaInicio = 0;
    uint8_t      _filaFim    = 0;
    uint8_t      _filaTamanho = 0;

    void _enfileirar(const char* topico, const char* payload);
    void _drenaFila();
    bool _publicarDireto(const char* topico, const char* payload);

    // -----------------------------------------------------------------------
    // Callbacks registrados
    // -----------------------------------------------------------------------

    CallbackMensagemMQTT _cbMensagem             = nullptr;
    CallbackWiFi         _cbWiFiConectado        = nullptr;
    CallbackWiFi         _cbWiFiDesconectado     = nullptr;
    CallbackMQTT         _cbMQTTConectado        = nullptr;
    CallbackMQTT         _cbMQTTDesconectado     = nullptr;

    // -----------------------------------------------------------------------
    // Callback interno do PubSubClient (estático — necessário pela API)
    // -----------------------------------------------------------------------

    static ESP32Connectivity* _instancia;
    static void _callbackMQTTEstatico(char* topico, byte* payload, unsigned int tamanho);
    void        _callbackMQTT(char* topico, byte* payload, unsigned int tamanho);
};

/** @brief Instância global pré-declarada para uso direto. */
extern ESP32Connectivity conectividade;

#endif // ESP32_CONNECTIVITY_H
