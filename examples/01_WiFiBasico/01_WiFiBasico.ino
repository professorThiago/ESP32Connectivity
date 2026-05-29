/**
 * @file 01_WiFiBasico.ino
 * @brief ESP32Connectivity — Exemplo 1: Conexão WiFi não-bloqueante.
 *
 * @details
 * Demonstra que o setup() retorna imediatamente mesmo sem WiFi disponível.
 * O `loop()` continua executando normalmente enquanto a conexão acontece
 * em segundo plano via `update()`.
 *
 * Observe no Monitor Serial que o contador continua incrementando
 * mesmo antes do WiFi conectar.
 *
 * @author  professorThiago (https://github.com/professorThiago)
 * @license MIT
 */

#include <Arduino.h>
#include "DebugManager.h"
#include "ESP32Connectivity.h"

uint32_t ultimoLog   = 0;
uint32_t contador    = 0;

void aoConectarWiFi() {
    debugInfo(">>> Callback: WiFi conectou! IP = " + WiFi.localIP().toString());
}

void aoDesconectarWiFi() {
    debugErro(">>> Callback: WiFi caiu!");
}

void setup() {
    configurarDebug();

    // Registra callbacks antes do begin()
    conectividade.registrarCallbackWiFiConectado(aoConectarWiFi);
    conectividade.registrarCallbackWiFiDesconectado(aoDesconectarWiFi);

    // begin() não bloqueia — setup() retorna imediatamente
    conectividade.beginTLS();

    debugInfo("setup() concluído. Loop iniciando...\n");
}

void loop() {
    // Avança a máquina de estado — NÃO bloqueia
    conectividade.update();

    // O programa principal continua funcionando normalmente
    if (millis() - ultimoLog >= 2000) {
        ultimoLog = millis();
        contador++;

        String estado = conectividade.wifiConectado() ? "CONECTADO" : "aguardando...";
        debugInfo("[" + String(contador) + "] WiFi: " + estado);
    }
}
