/**
 * @file secrets.cpp
 * @brief Credenciais e configurações do projeto.
 *
 * ╔══════════════════════════════════════════════════════════╗
 * ║  ATENÇÃO: este arquivo NÃO deve ir para o GitHub!       ║
 * ║  Adicione ao .gitignore do seu projeto:                 ║
 * ║      src/secrets.cpp                                    ║
 * ╚══════════════════════════════════════════════════════════╝
 *
 * Copie este arquivo para src/secrets.cpp e preencha os campos.
 */

#include "secrets.h"

// ── WiFi ─────────────────────────────────────────────────────
const char* WIFI_SSID  = "NOME_DA_REDE";
const char* WIFI_SENHA = "SENHA_DA_REDE";

// ── MQTT ─────────────────────────────────────────────────────
// Endereço do broker (hostname ou IP)
const char* MQTT_BROKER    = "broker.exemplo.com";

// Porta: 1883 sem TLS, 8883 com TLS
const int   MQTT_PORTA     = 8883;

// ID único deste dispositivo (sem espaços)
const char* MQTT_CLIENT_ID = "esp32_meu_projeto";

// Deixe "" para conexão anônima
const char* MQTT_USUARIO   = "";
const char* MQTT_SENHA     = "";

// Certificado CA do broker em PEM (necessário se USAR_AWS_IOT = false e TLS = true)
// Deixe vazio para usar setInsecure() — APENAS PARA TESTES
const char MQTT_CERTIFICADO_CA[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----

-----END CERTIFICATE-----
)EOF";

// ── AWS IoT Core ─────────────────────────────────────────────
// true  = usa AWS IoT Core com mTLS (ignora as configurações MQTT acima)
// false = usa broker MQTT comum
const bool USAR_AWS_IOT = false;

const char* AWS_IOT_ENDPOINT  = "XXXX.iot.us-east-1.amazonaws.com";
const int   AWS_IOT_PORT      = 8883;
const char* AWS_IOT_CLIENT_ID = "meu_thing_name";

// Certificado raiz da Amazon (igual para todos os projetos AWS)
const char AWS_CERT_CA[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----

-----END CERTIFICATE-----
)EOF";

// Certificado do dispositivo (baixado ao criar o Thing no AWS IoT Core)
const char AWS_CERT_CRT[] PROGMEM = R"CRT(
-----BEGIN CERTIFICATE-----

-----END CERTIFICATE-----
)CRT";

// Chave privada do dispositivo (baixada ao criar o Thing no AWS IoT Core)
const char AWS_CERT_PRIVATE[] PROGMEM = R"KEY(
-----BEGIN RSA PRIVATE KEY-----

-----END RSA PRIVATE KEY-----
)KEY";

// ── Tópicos MQTT ─────────────────────────────────────────────
// Tópicos nos quais o ESP32 publica — acesse por índice: publicar(0, "msg")
const char* TOPICOS_PUBLICAR[] = {
    "meu_projeto/esp32/status",   // índice 0 — reservado para anunciar conexão
    "meu_projeto/esp32/dados",    // índice 1
    "meu_projeto/esp32/log",      // índice 2
};

// Tópicos nos quais o ESP32 se inscreve para receber mensagens
const char* TOPICOS_RECEBER[] = {
    "meu_projeto/esp32/comando",  // índice 0
    "meu_projeto/esp32/config",   // índice 1
};

const int TOTAL_TOPICOS_PUBLICAR = sizeof(TOPICOS_PUBLICAR) / sizeof(TOPICOS_PUBLICAR[0]);
const int TOTAL_TOPICOS_RECEBER  = sizeof(TOPICOS_RECEBER)  / sizeof(TOPICOS_RECEBER[0]);

// ── Debug ─────────────────────────────────────────────────────
// Nível de log padrão ao ligar:
//   0 = DEBUG_NENHUM  — silencioso (produção)
//   1 = DEBUG_ERRO    — apenas erros críticos
//   2 = DEBUG_AVISO   — erros + avisos de reconexão
//   3 = DEBUG_INFO    — fluxo normal (recomendado para desenvolvimento)
//   4 = DEBUG_VERBOSE — detalhes internos (tentativas, estados)
//   5 = DEBUG_TUDO    — payloads e códigos de erro brutos
const int DEBUG_NIVEL_INICIAL = 3;

// GPIO com pull-up interno. Conecte ao GND antes de ligar para forçar DEBUG_TUDO.
// Use -1 para desabilitar.
const int PINO_HABILITA_DEBUG_COMPLETO = 4;
