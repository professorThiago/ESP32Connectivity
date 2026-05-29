/**
 * @file 02_MQTTBasico.ino
 * @brief ESP32Connectivity — Exemplo 2: WiFi + MQTT com callbacks.
 *
 * @details
 * Conecta ao broker MQTT configurado em secrets.h e demonstra:
 * - Publicação via índice de tópico.
 * - Recebimento de mensagens via callback.
 * - Verificação de estado sem bloquear o loop.
 *
 * @author  professorThiago (https://github.com/professorThiago)
 * @license MIT
 */

#include <Arduino.h>
#include "DebugManager.h"
#include "ESP32Connectivity.h"

uint32_t ultimaPublicacao = 0;
uint32_t contador = 0;

// ── Callbacks de eventos ─────────────────────────────────────

void aoConectarMQTT() {
    debugInfo(">>> MQTT conectado! Publicando mensagem de presença...");
}

void aoDesconectarMQTT() {
    debugErro(">>> MQTT desconectado.");
}

void aoReceberMensagem(const char* topico, const String& mensagem) {
    debugInfo(">>> Mensagem recebida!");
    debugInfo("    Tópico  : " + String(topico));
    debugInfo("    Payload : " + mensagem);

    // Exemplo de roteamento por tópico
    if (mensagem == "ligar") {
        debugInfo("    Ação: ligar dispositivo");
    } else if (mensagem == "desligar") {
        debugInfo("    Ação: desligar dispositivo");
    }
}

// ── Setup e loop ─────────────────────────────────────────────

void setup() {
    configurarDebug();

    conectividade.registrarCallbackMQTTConectado(aoConectarMQTT);
    conectividade.registrarCallbackMQTTDesconectado(aoDesconectarMQTT);
    conectividade.registrarCallbackMensagem(aoReceberMensagem);

    conectividade.beginTLS();

    debugInfo("Setup concluído.\n");
}

void loop() {
    conectividade.update();

    // Publica a cada 10 segundos (apenas se MQTT conectado)
    if (conectividade.mqttConectado() && millis() - ultimaPublicacao >= 10000) {
        ultimaPublicacao = millis();
        contador++;

        String payload = "{\"contador\":" + String(contador) + ",\"uptime\":" + String(millis() / 1000) + "}";
        conectividade.publicar(0, payload.c_str());
    }
}
