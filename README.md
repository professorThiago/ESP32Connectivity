# ESP32Connectivity

Biblioteca Arduino/ESP32 para gerenciamento **não-bloqueante** de WiFi e MQTT, com reconexão automática e fila de mensagens offline.

> Autor: [professorThiago](https://github.com/professorThiago)  
> Depende de: [DebugManager](https://github.com/professorThiago/DebugManager)

---

## Por que não-bloqueante?

A abordagem tradicional usa `while()` para aguardar conexão:

```cpp
// ❌ Bloqueante — trava o setup() por até 15 segundos
while (WiFi.status() != WL_CONNECTED) {
    delay(500);
}
```

Esta biblioteca usa **máquinas de estado** processadas no `loop()`:

```cpp
// ✓ Não-bloqueante — setup() retorna imediatamente
conectividade.begin();   // inicia a conexão, não espera
// ...
void loop() {
    conectividade.update();   // avança os estados sem travar
    // seu código continua executando normalmente
}
```

Isso permite que o programa grave dados em memória, pisque LEDs, leia sensores ou execute qualquer outra lógica enquanto aguarda a conexão.

---

## Funcionalidades

- ✅ Conexão WiFi não-bloqueante com reconexão automática.
- ✅ Conexão MQTT não-bloqueante com reconexão automática.
- ✅ Suporte a MQTT simples, MQTT com TLS e **AWS IoT Core** (mTLS).
- ✅ **Fila offline:** mensagens publicadas sem MQTT conectado são salvas e enviadas automaticamente ao reconectar.
- ✅ Callbacks para todos os eventos: WiFi conectado/desconectado, MQTT conectado/desconectado, mensagem recebida.

---

## Instalação

```ini
lib_deps =
    https://github.com/professorThiago/ESP32Connectivity
    https://github.com/professorThiago/DebugManager
    knolleary/PubSubClient @ ^2.8
```

---

## Configuração (secrets.h / secrets.cpp)

Toda a configuração fica em `secrets.h` e `secrets.cpp` no projeto, fora da biblioteca:

```cpp
// WiFi
const char* WIFI_SSID  = "MinhaRede";
const char* WIFI_SENHA = "minha_senha";

// MQTT (broker comum com TLS)
const char* MQTT_BROKER    = "broker.exemplo.com";
const int   MQTT_PORTA     = 8883;
const char* MQTT_CLIENT_ID = "esp32_meu_projeto";
const char* MQTT_USUARIO   = "usuario";
const char* MQTT_SENHA     = "senha";
const bool  MQTT_TLS       = true;
const char  MQTT_CERTIFICADO_CA[] PROGMEM = R"EOF( ... )EOF";

// AWS IoT Core (define USAR_AWS_IOT = true e preencha os certificados)
const bool  USAR_AWS_IOT       = false;
const char* AWS_IOT_ENDPOINT   = "xxxx.iot.us-east-1.amazonaws.com";
const int   AWS_IOT_PORT       = 8883;
const char* AWS_IOT_CLIENT_ID  = "meu_dispositivo";
const char  AWS_CERT_CA[]      PROGMEM = R"EOF( ... )EOF";
const char  AWS_CERT_CRT[]     PROGMEM = R"CRT( ... )CRT";
const char  AWS_CERT_PRIVATE[] PROGMEM = R"KEY( ... )KEY";

// Tópicos
const char* TOPICOS_PUBLICAR[] = { "meu/topico/status", "meu/topico/log" };
const char* TOPICOS_RECEBER[]  = { "meu/topico/comando" };
const int TOTAL_TOPICOS_PUBLICAR = sizeof(TOPICOS_PUBLICAR) / sizeof(TOPICOS_PUBLICAR[0]);
const int TOTAL_TOPICOS_RECEBER  = sizeof(TOPICOS_RECEBER)  / sizeof(TOPICOS_RECEBER[0]);
```

---

## Quick start

```cpp
#include <DebugManager.h>
#include <ESP32Connectivity.h>

void aoReceberMensagem(const char* topico, const String& msg) {
    Serial.println("Recebi em " + String(topico) + ": " + msg);
}

void setup() {
    configurarDebug();
    conectividade.registrarCallbackMensagem(aoReceberMensagem);
    conectividade.begin();   // não bloqueia
}

void loop() {
    conectividade.update();   // sempre no loop()

    // Publica mesmo offline — vai para a fila automaticamente
    conectividade.publicar(0, "status: ok");
    delay(5000);
}
```

---

## Fila de mensagens offline

Quando o MQTT está desconectado, qualquer chamada a `publicar()` **salva a mensagem na fila** em vez de descartá-la. Ao reconectar, a fila é drenada automaticamente.

```cpp
// Se offline, esta mensagem vai para a fila
conectividade.publicar(0, "leitura: 23.5°C");

// Verifica quantas mensagens estão aguardando
Serial.println("Na fila: " + String(conectividade.mensagensNaFila()));
```

| Parâmetro | Padrão | Como alterar |
|-----------|--------|--------------|
| Tamanho da fila | 10 mensagens | `#define CONNECTIVITY_FILA_TAMANHO 20` antes do `#include` |
| Tamanho do tópico | 64 bytes | `#define CONNECTIVITY_FILA_TOPICO_MAX 128` |
| Tamanho do payload | 256 bytes | `#define CONNECTIVITY_FILA_PAYLOAD_MAX 512` |

---

## API

### Inicialização

#### `void begin()`
Inicia WiFi e MQTT de forma não-bloqueante. Chame uma vez no `setup()`.

#### `void update()`
**Deve ser chamado em todo `loop()`**. Avança as máquinas de estado WiFi e MQTT, processa mensagens recebidas e drena a fila offline.

---

### Estado

| Método | Retorno |
|--------|---------|
| `wifiConectado()` | `true` se WiFi está conectado |
| `mqttConectado()` | `true` se MQTT está conectado |
| `mensagensNaFila()` | Número de mensagens aguardando envio |

---

### Publicação

#### `bool publicar(int indiceTopico, const char* mensagem)`
Publica no tópico `TOPICOS_PUBLICAR[indiceTopico]`. Se offline, enfileira.  
Retorna `true` se publicado imediatamente.

#### `bool publicar(const char* topico, const char* mensagem)`
Publica em um tópico pelo nome completo. Mesma lógica de enfileiramento.

---

### Callbacks

```cpp
// Mensagem recebida via MQTT
conectividade.registrarCallbackMensagem(
    [](const char* topico, const String& msg) { ... }
);

// Eventos de conexão
conectividade.registrarCallbackWiFiConectado(   []() { ... } );
conectividade.registrarCallbackWiFiDesconectado([]() { ... } );
conectividade.registrarCallbackMQTTConectado(   []() { ... } );
conectividade.registrarCallbackMQTTDesconectado([]() { ... } );
```

---

## Exemplos

| Exemplo | Descrição |
|---------|-----------|
| `01_WiFiBasico` | Conexão WiFi não-bloqueante com callbacks |
| `02_MQTTBasico` | WiFi + MQTT com publicação periódica e recebimento |
| `03_FilaOffline` | Demonstra enfileiramento e drenagem automática |

---

## Máquinas de estado

### WiFi

```
DESCONECTADO ──begin()──> CONECTANDO ──sucesso──> CONECTADO
     ^                        |                       |
     |                   timeout (15s)            queda de sinal
     └──────── retry a cada 5s ──────────────────────┘
```

### MQTT

```
DESCONECTADO ──WiFi OK──> CONECTANDO ──sucesso──> CONECTADO
     ^                        |                       |
     |                   falha na tentativa       broker caiu
     └──── retry a cada 5s (máx. 5 tentativas) ───────┘
```

Após 5 tentativas falhas consecutivas, o MQTT aguarda 30 segundos antes de tentar novamente, evitando flood no broker.

---

## Licença

MIT © 2026 [professorThiago](https://github.com/professorThiago)
