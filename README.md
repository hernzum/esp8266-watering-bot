# ESP8266 Watering Bot

Un bot de Telegram que controla una bomba de riego basada en la humedad del suelo usando un ESP8266. Este proyecto te envía un mensaje cuando el ESP se inicia y permite controlar y monitorear el riego de tus plantas.

## Partes Necesarias

- D1 Mini ESP8266 (o cualquier placa ESP8266)
- Sensor de humedad del suelo
- Bomba de riego
- Pantalla LCD I2C

## Funcionalidades

- Monitoreo del nivel de humedad del suelo
- Control automático de la bomba de riego
- Interacción con el bot de Telegram para obtener información y controlar el riego

## Comandos de Telegram

- `/humedad`: Muestra el nivel actual de humedad del suelo.
- `/plantas`: Muestra una lista de plantas disponibles.
- `/setplanta <nombre>`: Establece la planta a monitorear.
- `/plantainfo`: Muestra la planta actual y sus parámetros.
- `/info`: Muestra las instrucciones para usar el bot.

## Instalación

1. Clona el repositorio:
    ```bash
    git clone https://github.com/hernzum/esp8266-watering-bot.git
    cd esp8266-watering-bot
    ```

2. Configura las credenciales de WiFi y el token del bot de Telegram en el archivo del código fuente:
    ```cpp
    const char* BOT_TOKEN = "TU_BOT_TOKEN";
    const char* CHAT_ID = "TU_CHAT_ID";
    ```

3. Sube el código al ESP8266 usando el IDE de Arduino o la plataforma de tu preferencia.

## Uso

1. Conecta el ESP8266, el sensor de humedad, la bomba de riego y la pantalla LCD I2C según el esquema de conexión.

2. Abre un chat con tu bot de Telegram y usa los comandos disponibles para interactuar con el sistema de riego.

## Contribuir

1. Haz un fork del proyecto.
2. Crea una nueva rama (`git checkout -b feature/nueva-funcionalidad`).
3. Realiza tus cambios y haz commit (`git commit -am 'Añadir nueva funcionalidad'`).
4. Sube tu rama (`git push origin feature/nueva-funcionalidad`).
5. Crea un nuevo Pull Request.

## Licencia

Este proyecto está bajo la Licencia MIT. Consulta el archivo [LICENSE](LICENSE) para más detalles.

## Versión

1.2

## Autor

Hernzum - [GitHub](https://github.com/hernzum)
