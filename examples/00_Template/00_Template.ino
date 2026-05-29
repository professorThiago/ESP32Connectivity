/**
 * @file 00_Template.ino
 * @brief ESP32Connectivity — Template para copiar e parametrizar.
 *
 * @details
 * Ponto de partida para qualquer projeto que use WiFi + MQTT no ESP32.
 *
 * COMO USAR:
 *  1. Copie secrets.h e secrets.cpp para a pasta src/ do seu projeto.
 *  2. Preencha os campos "PREENCHER" em secrets.cpp.
 *  3. Adicione src/secrets.cpp ao .gitignore do seu projeto.
 *  4. Implemente as funções de callback abaixo conforme sua necessidade.
 *  5. Adicione sua lógica de negócio na função loopAplicacao().
 *
 * ESTRUTURA DO PROJETO:
 * @code
 *   meu_projeto/
 *   ├── platformio.ini
 *   ├── .gitignore          ← adicione: src/secrets.cpp
 *   └── src/
 *       ├── main.cpp        ← este arquivo
 *       ├── secrets.h       ← copiado de examples/00_Template/
 *       └── secrets.cpp     ← copiado de examples/00_Template/ e preenchido
 * @endcode
 *
 * PLATFORMIO.INI:
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
 * @license MIT
 */

#include <Arduino.h>
#include "DebugManager.h"
#include "ESP32Connectivity.h"

// =============================================================================
// Callbacks de eventos — implemente conforme sua necessidade
// =============================================================================

/**
 * Chamado quando o WiFi conecta ou reconecta.
 * Use para reiniciar periféricos que dependem de rede.
 */
void aoConectarWiFi()
{
    debugInfo("WiFi conectado.");
}

/**
 * Chamado quando o WiFi cai.
 * Use para salvar estado local ou parar operações de rede.
 */
void aoDesconectarWiFi()
{
    debugAviso("WiFi desconectado.");
}

/**
 * Chamado quando o MQTT conecta ou reconecta.
 * Use para publicar estado inicial ou reinscrever em tópicos dinâmicos.
 */
void aoConectarMQTT()
{
    debugInfo("MQTT conectado.");
    conectividade.publicar(0, "{\"status\":\"online\"}");
}

/**
 * Chamado quando o MQTT desconecta.
 */
void aoDesconectarMQTT()
{
    debugAviso("MQTT desconectado.");
}

/**
 * Chamado ao receber uma mensagem em qualquer tópico inscrito.
 * Implemente o roteamento por tópico conforme sua aplicação.
 *
 * @param topico    Tópico no qual a mensagem foi recebida.
 * @param mensagem  Payload da mensagem.
 */
void aoReceberMensagem(const char* topico, const String& mensagem)
{
    debugInfo("Mensagem recebida em: " + String(topico));
    debugTudo("Payload: " + mensagem);

    // Exemplo de roteamento por tópico
    // if (String(topico) == conectividade.topicoRecebimento(0)) {
    //     // processa comando
    // }
}

// =============================================================================
// Lógica da aplicação — coloque aqui o código do seu projeto
// =============================================================================

uint32_t ultimaPublicacao = 0;

void loopAplicacao()
{
    // Exemplo: publica leitura a cada 10 segundos
    if (millis() - ultimaPublicacao >= 10000)
    {
        ultimaPublicacao = millis();

        // Substitua pelo dado real do seu projeto
        String payload = "{\"uptime\":" + String(millis() / 1000) + "}";

        // publicar() enfileira automaticamente se o MQTT estiver offline
        conectividade.publicar(1, payload.c_str());
    }
}

// =============================================================================
// Setup e loop
// =============================================================================

void setup()
{
    configurarDebug();

    // Registra os callbacks antes de iniciar a conexão
    conectividade.registrarCallbackWiFiConectado(aoConectarWiFi);
    conectividade.registrarCallbackWiFiDesconectado(aoDesconectarWiFi);
    conectividade.registrarCallbackMQTTConectado(aoConectarMQTT);
    conectividade.registrarCallbackMQTTDesconectado(aoDesconectarMQTT);
    conectividade.registrarCallbackMensagem(aoReceberMensagem);

    // Inicia WiFi e MQTT de forma não-bloqueante
    conectividade.begin();

    debugInfo("Setup concluido. Aguardando conexao...");
}

void loop()
{
    // Avança as máquinas de estado WiFi e MQTT — NUNCA remova esta linha
    conectividade.update();

    // Sua lógica de aplicação
    loopAplicacao();
}
