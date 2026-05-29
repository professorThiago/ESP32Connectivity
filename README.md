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
- ✅ Diagnóstico detalhado: códigos de erro WiFi e MQTT traduzidos em português com dicas de solução.

---

## Instalação

```ini
lib_deps =
    https://github.com/professorThiago/ESP32Connectivity
```

> A biblioteca baixa automaticamente o **DebugManager** e o **PubSubClient** como dependências.

---

## Configurando um projeto novo

### 1. Estrutura de pastas

```
meu_projeto/
├── platformio.ini
├── .gitignore          ← adicione: src/secrets.cpp
└── src/
    ├── main.cpp
    ├── secrets.h       ← copie de examples/00_Template/
    └── secrets.cpp     ← copie de examples/00_Template/ e preencha
```

### 2. Copie os arquivos de template

Na pasta `examples/00_Template/` desta biblioteca há três arquivos prontos:

| Arquivo | O que fazer |
|---------|-------------|
| `secrets.h` | Copie para `src/secrets.h` — não edite |
| `secrets.cpp` | Copie para `src/secrets.cpp` e preencha os campos `"PREENCHER"` |
| `00_Template.ino` | Copie para `src/main.cpp` e implemente os callbacks |

### 3. Proteja suas credenciais

Adicione ao `.gitignore` do projeto:

```
src/secrets.cpp
```

### 4. Preencha o secrets.cpp

Os campos principais:

```cpp
// WiFi
const char* WIFI_SSID  = "MinhaRede";
const char* WIFI_SENHA = "minha_senha";

// Nível de debug — 3 (INFO) é um bom padrão para desenvolvimento
const int DEBUG_NIVEL_INICIAL = 3;

// MQTT com TLS
const char* MQTT_BROKER    = "broker.exemplo.com";
const int   MQTT_PORTA     = 8883;
const char* MQTT_CLIENT_ID = "esp32_meu_projeto";
const char* MQTT_USUARIO   = "usuario";
const char* MQTT_SENHA     = "senha";
const bool  MQTT_TLS       = true;

// OU: AWS IoT Core — defina USAR_AWS_IOT = true e cole os certificados
const bool USAR_AWS_IOT = false;
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
| `00_Template` | **Ponto de partida** — copie para o seu projeto e parametrize |
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
