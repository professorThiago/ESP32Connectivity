/**
 * @file secrets.cpp
 * @brief Configurações privadas do projeto — NÃO suba para o GitHub!
 *
 * Copie este arquivo para src/secrets.cpp no seu projeto e preencha
 * os campos marcados com "PREENCHER".
 *
 * Adicione ao .gitignore do seu projeto:
 *   src/secrets.cpp
 */

#include "secrets.h"
#include <Arduino.h>

// =============================
// WiFi
// =============================

const char* WIFI_SSID  = "PREENCHER";   // nome da rede WiFi
const char* WIFI_SENHA = "PREENCHER";   // senha da rede WiFi

// =============================
// MQTT
// =============================

// Endereço do broker MQTT (ex: "broker.hivemq.com" ou IP local)
const char* MQTT_BROKER    = "PREENCHER";
const int   MQTT_PORTA     = 8883;          // 1883 sem TLS, 8883 com TLS

// ID único deste dispositivo no broker (sem espaços)
const char* MQTT_CLIENT_ID = "esp32_meu_projeto";

// Deixe vazio ("") para conexão anônima
const char* MQTT_USUARIO   = "PREENCHER";
const char* MQTT_SENHA     = "PREENCHER";

// true  = conexão criptografada (recomendado)
// false = sem criptografia (apenas redes locais confiáveis)
const bool  MQTT_TLS       = true;

// Certificado CA do broker MQTT (apenas se MQTT_TLS = true e não for AWS)
// Deixe vazio ("") para usar setInsecure() — APENAS PARA TESTES
const char MQTT_CERTIFICADO_CA[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----

-----END CERTIFICATE-----
)EOF";

// =============================
// AWS IoT Core
// =============================

// true  = usa AWS IoT Core com mTLS (ignora as configurações MQTT acima)
// false = usa broker MQTT comum configurado acima
const bool USAR_AWS_IOT = false;

// Endpoint do AWS IoT Core (Console AWS → IoT Core → Configurações)
const char* AWS_IOT_ENDPOINT  = "PREENCHER.iot.us-east-1.amazonaws.com";
const int   AWS_IOT_PORT      = 8883;
const char* AWS_IOT_CLIENT_ID = "PREENCHER";

// Certificado raiz da Amazon (mesmo para todos os projetos AWS)
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

// =============================
// Tópicos MQTT
// =============================

// Tópicos nos quais este ESP32 publica mensagens
// Acesse por índice: conectividade.publicar(0, "msg") → publica no primeiro tópico
const char* TOPICOS_PUBLICAR[] = {
    "meu_projeto/esp32/status",     // índice 0 — usado para anunciar conexão
    "meu_projeto/esp32/dados",      // índice 1
    "meu_projeto/esp32/log",        // índice 2
};

// Tópicos nos quais este ESP32 se inscreve para receber mensagens
// As mensagens chegam via callback registrado em registrarCallbackMensagem()
const char* TOPICOS_RECEBER[] = {
    "meu_projeto/esp32/comando",    // índice 0
    "meu_projeto/esp32/config",     // índice 1
};

const int TOTAL_TOPICOS_PUBLICAR = sizeof(TOPICOS_PUBLICAR) / sizeof(TOPICOS_PUBLICAR[0]);
const int TOTAL_TOPICOS_RECEBER  = sizeof(TOPICOS_RECEBER)  / sizeof(TOPICOS_RECEBER[0]);

// =============================
// Debug
// =============================

// Nível de log padrão ao ligar:
//   0 = DEBUG_NENHUM  — silencioso (produção)
//   1 = DEBUG_ERRO    — apenas erros críticos
//   2 = DEBUG_AVISO   — erros + avisos de reconexão
//   3 = DEBUG_INFO    — fluxo normal (conectou, publicou)
//   4 = DEBUG_VERBOSE — detalhes internos (tentativas, estados)
//   5 = DEBUG_TUDO    — dados brutos e códigos de erro
const int DEBUG_NIVEL_INICIAL = 3;

// Pino com pull-up interno. Conecte ao GND antes de ligar para forçar DEBUG_TUDO.
// Use -1 para desabilitar este recurso.
const int PINO_HABILITA_DEBUG_COMPLETO = 4;
