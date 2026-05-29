/**
 * @file 00_Template.ino
 * @brief ESP32Connectivity — Template completo para copiar e parametrizar.
 *
 * COMO USAR:
 *  1. Copie secrets.h  → src/secrets.h  (pode ir pro GitHub — sem dados)
 *  2. Copie secrets.cpp → src/secrets.cpp e preencha as credenciais
 *  3. Adicione ao .gitignore:  src/secrets.cpp
 *  4. Copie este arquivo → src/main.cpp
 *  5. Descomente o begin correspondente ao seu modo de conexão
 *  6. Implemente os callbacks e a loopAplicacao()
 *
 * ESTRUTURA DO PROJETO:
 *   meu_projeto/
 *   ├── platformio.ini
 *   ├── .gitignore          ← inclua: src/secrets.cpp
 *   └── src/
 *       ├── main.cpp        ← este arquivo
 *       ├── secrets.h       ← copiado do template (vai pro GitHub)
 *       └── secrets.cpp     ← copiado do template e preenchido (NÃO vai pro GitHub)
 *
 * PLATFORMIO.INI:
 *   [env:esp32-s3-devkitm-1]
 *   platform  = espressif32
 *   board     = esp32-s3-devkitm-1
 *   framework = arduino
 *   lib_deps  = https://github.com/professorThiago/ESP32Connectivity
 *
 * @author  professorThiago (https://github.com/professorThiago)
 * @license MIT
 */

#include <Arduino.h>
#include "secrets.h"
#include "DebugManager.h"
#include "ESP32Connectivity.h"

ConfigTopicos topicos = {
    TOPICOS_PUBLICAR, TOTAL_TOPICOS_PUBLICAR,
    TOPICOS_RECEBER,  TOTAL_TOPICOS_RECEBER
};

// =============================================================================
// CALLBACKS
// =============================================================================

void aoConectarWiFi()    { debugInfo("WiFi conectado.");      }
void aoDesconectarWiFi() { debugAviso("WiFi desconectado.");  }
void aoConectarMQTT()    {
    debugInfo("MQTT conectado.");
    conectividade.publicar(0, "{\"status\":\"online\"}");
}
void aoDesconectarMQTT() { debugAviso("MQTT desconectado."); }

void aoReceberMensagem(const char* topico, const String& mensagem) {
    debugInfo("Mensagem em: " + String(topico));
    debugTudo("Payload: " + mensagem);
    // if (String(topico) == conectividade.topicoRecebimento(0)) { ... }
}

// =============================================================================
// LÓGICA DA APLICAÇÃO
// =============================================================================

uint32_t ultimaPublicacao = 0;

void loopAplicacao() {
    if (millis() - ultimaPublicacao >= 10000) {
        ultimaPublicacao = millis();
        String payload = "{\"uptime\":" + String(millis() / 1000) + "}";
        conectividade.publicar(1, payload.c_str());
    }
}

// =============================================================================
// SETUP
// =============================================================================

void setup() {
    configurarDebug(DEBUG_NIVEL_INICIAL, PINO_HABILITA_DEBUG_COMPLETO);

    // conectividade.configurarBufferMQTT(1024);  // opcional: payloads > 256 bytes

    conectividade.registrarCallbackWiFiConectado(aoConectarWiFi);
    conectividade.registrarCallbackWiFiDesconectado(aoDesconectarWiFi);
    conectividade.registrarCallbackMQTTConectado(aoConectarMQTT);
    conectividade.registrarCallbackMQTTDesconectado(aoDesconectarMQTT);
    conectividade.registrarCallbackMensagem(aoReceberMensagem);

    // ── Descomente APENAS UM dos três modos abaixo ───────────────────────────

    // MODO 1: sem criptografia (redes locais, porta 1883)
    conectividade.beginSimples(
        { WIFI_SSID, WIFI_SENHA },
        { MQTT_BROKER, MQTT_PORTA, MQTT_CLIENT_ID, MQTT_USUARIO, MQTT_SENHA },
        topicos
    );

    // MODO 2: TLS — broker público com certificado CA (porta 8883)
    // conectividade.beginTLS(
    //     { WIFI_SSID, WIFI_SENHA },
    //     { MQTT_BROKER, MQTT_PORTA, MQTT_CLIENT_ID, MQTT_USUARIO, MQTT_SENHA },
    //     { MQTT_CERTIFICADO_CA },
    //     topicos
    // );

    // MODO 3: AWS IoT Core — mTLS com certificado de dispositivo (porta 8883)
    // conectividade.beginAWS(
    //     { WIFI_SSID, WIFI_SENHA },
    //     { AWS_IOT_ENDPOINT, AWS_IOT_PORT, AWS_IOT_CLIENT_ID,
    //       AWS_CERT_CA, AWS_CERT_CRT, AWS_CERT_PRIVATE },
    //     topicos
    // );

    debugInfo("Setup concluido.");
}

// =============================================================================
// LOOP
// =============================================================================

void loop() {
    conectividade.update();
    loopAplicacao();
}
