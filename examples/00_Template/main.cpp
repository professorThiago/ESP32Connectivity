/**
 * @file main.cpp
 * @brief Template de projeto usando ESP32Connectivity + DebugManager.
 *
 * @details
 * Copie este arquivo e secrets.h / secrets.cpp para src/ do seu projeto.
 * Preencha secrets.cpp com seus dados e adapte as funções marcadas com
 * TODO abaixo.
 *
 * Estrutura do projeto:
 * @code
 *   meu_projeto/
 *   ├── src/
 *   │   ├── main.cpp        ← este arquivo
 *   │   ├── secrets.h       ← copie de examples/00_Template/
 *   │   └── secrets.cpp     ← copie e preencha com seus dados
 *   └── platformio.ini
 * @endcode
 *
 * platformio.ini mínimo:
 * @code
 *   [env:esp32-s3-devkitm-1]
 *   platform  = espressif32
 *   board     = esp32-s3-devkitm-1
 *   framework = arduino
 *
 *   lib_deps =
 *       https://github.com/professorThiago/ESP32Connectivity
 * @endcode
 *
 * @author  professorThiago (https://github.com/professorThiago)
 */

#include <Arduino.h>
#include "DebugManager.h"
#include "ESP32Connectivity.h"

// ============================================================
// Configuração de tempo
// ============================================================

// Intervalo entre publicações (ms)
const uint32_t INTERVALO_PUBLICACAO_MS = 10000;

// ============================================================
// Variáveis de controle do loop
// ============================================================

uint32_t ultimaPublicacao = 0;

// ============================================================
// Callbacks de eventos de conectividade
// ============================================================

void aoConectarWiFi() {
    debugInfo("WiFi conectado. IP: " + WiFi.localIP().toString());
    // TODO: adicione ações ao conectar o WiFi (acender LED, etc.)
}

void aoDesconectarWiFi() {
    debugAviso("WiFi desconectado.");
    // TODO: adicione ações ao perder o WiFi (apagar LED, etc.)
}

void aoConectarMQTT() {
    debugInfo("MQTT conectado.");
    // TODO: adicione ações ao conectar o MQTT
}

void aoDesconectarMQTT() {
    debugAviso("MQTT desconectado.");
    // TODO: adicione ações ao perder o MQTT
}

// ============================================================
// Callback de mensagens recebidas
// ============================================================

void aoReceberMensagem(const char* topico, const String& mensagem) {
    debugInfo("Mensagem recebida");
    debugVerbose("Topico : " + String(topico));
    debugTudo   ("Payload: " + mensagem);

    // TODO: implemente o roteamento por tópico abaixo.
    // Use conectividade.topicoRecebimento(indice) para comparar
    // sem escrever o nome do tópico diretamente no código.

    if (String(topico) == conectividade.topicoRecebimento(0)) {
        // TODO: processe o comando recebido no tópico 0
        debugInfo("Comando recebido: " + mensagem);
    }

    else if (String(topico) == conectividade.topicoRecebimento(1)) {
        // TODO: processe o comando recebido no tópico 1
        debugInfo("Config recebida: " + mensagem);
    }
}

// ============================================================
// Leitura de dados / lógica da aplicação
// ============================================================

String lerDados() {
    // TODO: substitua pelo código real de leitura (sensor, etc.)
    // Retorne um JSON ou qualquer string que queira publicar.
    return "{\"uptime\":" + String(millis() / 1000) + "}";
}

// ============================================================
// Setup
// ============================================================

void setup() {
    configurarDebug();   // sempre primeiro

    // TODO: inicialize periféricos aqui (pinos, sensores, displays...)

    // Registra callbacks antes do begin()
    conectividade.registrarCallbackWiFiConectado(aoConectarWiFi);
    conectividade.registrarCallbackWiFiDesconectado(aoDesconectarWiFi);
    conectividade.registrarCallbackMQTTConectado(aoConectarMQTT);
    conectividade.registrarCallbackMQTTDesconectado(aoDesconectarMQTT);
    conectividade.registrarCallbackMensagem(aoReceberMensagem);

    // Inicia WiFi + MQTT (não bloqueia)
    conectividade.begin();

    debugInfo("Setup concluido. Aguardando conexao...");
}

// ============================================================
// Loop
// ============================================================

void loop() {
    // Avança WiFi, MQTT e drena fila offline — SEMPRE primeiro
    conectividade.update();

    // TODO: adicione aqui a lógica principal da aplicação.
    // Este bloco executa mesmo sem WiFi/MQTT conectados.

    // Publica periodicamente
    if (millis() - ultimaPublicacao >= INTERVALO_PUBLICACAO_MS) {
        ultimaPublicacao = millis();

        String dados = lerDados();

        // publicar() enfileira automaticamente se o MQTT estiver offline
        conectividade.publicar(0, dados.c_str());
    }
}
