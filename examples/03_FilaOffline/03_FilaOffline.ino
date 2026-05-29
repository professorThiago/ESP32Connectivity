/**
 * @file 03_FilaOffline.ino
 * @brief ESP32Connectivity — Exemplo 3: Fila de mensagens offline.
 *
 * @details
 * Demonstra que mensagens publicadas sem MQTT conectado são
 * **enfileiradas automaticamente** e enviadas ao reconectar.
 *
 * Fluxo do exemplo:
 *  1. Inicia sem esperar conexão.
 *  2. Publica mensagens a cada 3 segundos (vão para a fila se offline).
 *  3. Quando o MQTT conecta, a fila é drenada automaticamente.
 *  4. Exibe o tamanho atual da fila no Monitor Serial.
 *
 * Para testar: desligue o broker MQTT, aguarde algumas mensagens
 * enfileirarem, depois religue — a fila será enviada automaticamente.
 *
 * @author  professorThiago (https://github.com/professorThiago)
 * @license MIT
 */

#include <Arduino.h>
#include "DebugManager.h"
#include "ESP32Connectivity.h"

uint32_t ultimaPublicacao = 0;
uint32_t ultimoStatus     = 0;
uint32_t contadorMensagens = 0;

void aoConectarMQTT() {
    debugInfo("MQTT conectado! Fila será drenada automaticamente...");
}

void setup() {
    configurarDebug();
    conectividade.registrarCallbackMQTTConectado(aoConectarMQTT);
    conectividade.begin();
    debugInfo("Setup concluído. Publicando a cada 3s independente do estado.\n");
}

void loop() {
    conectividade.update();

    // Publica a cada 3s — se offline, vai para a fila
    if (millis() - ultimaPublicacao >= 3000) {
        ultimaPublicacao = millis();
        contadorMensagens++;

        String payload = "{\"seq\":" + String(contadorMensagens) +
                         ",\"ts\":" + String(millis()) + "}";

        bool enviado = conectividade.publicar(0, payload.c_str());

        if (enviado) {
            debugInfo("Mensagem " + String(contadorMensagens) + " enviada diretamente.");
        } else {
            debugInfo("Mensagem " + String(contadorMensagens) + " enfileirada. " +
                      "Fila: " + String(conectividade.mensagensNaFila()) +
                      "/" + String(CONNECTIVITY_FILA_TAMANHO));
        }
    }

    // Exibe status a cada 5s
    if (millis() - ultimoStatus >= 5000) {
        ultimoStatus = millis();
        debugInfo("--- Status ---");
        debugInfo("WiFi : " + String(conectividade.wifiConectado() ? "CONECTADO" : "OFFLINE"));
        debugInfo("MQTT : " + String(conectividade.mqttConectado() ? "CONECTADO" : "OFFLINE"));
        debugInfo("Fila : " + String(conectividade.mensagensNaFila()) + " mensagem(ns)");
        debugInfo("--------------");
    }
}
