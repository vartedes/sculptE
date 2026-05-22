# Guía Paso a Paso para Compilar SculptEngine a .EXE (PC) y .APK (Android)

Esta guía te proporcionará los pasos detallados para compilar y generar la versión final de la aplicación en tu propia máquina.

---

## 📋 Requisitos Previos

Antes de comenzar, asegúrate de tener instalado en tu computadora lo siguiente:

1.  **Node.js (versión 18 o superior):** Descárgalo e instálalo desde [nodejs.org](https://nodejs.org/). Esto instalará automáticamente `npm`.
2.  **Para Android (.APK):**
    *   **Java JDK (versión 17):** Necesario para la compilación de Android Gradle.
    *   **Android Studio:** Descárgalo de [developer.android.com](https://developer.android.com/studio). Instala las herramientas de Android SDK desde el SDK Manager dentro de Android Studio.

---

## 🚀 Paso 1: Instalar Dependencias del Proyecto

Abre tu terminal en la carpeta raíz del proyecto (`sculptengine-web (1)`) y ejecuta:

```bash
npm install
```

*Nota: Esto descargará todas las librerías de React, Three.js, Electron y Capacitor configuradas en el `package.json`.*

---

## 🖥️ Paso 2: Generar el Archivo Portable .EXE (PC Windows)

Hemos configurado `electron-builder` para que genere un ejecutable **Portable**. Esto significa que obtendrás un archivo `.exe` único que el usuario puede abrir directamente con doble clic, sin necesidad de instalación.

Para compilar el `.exe`, ejecuta en tu terminal:

```bash
npm run app:dist:win
```

### ¿Qué hace este comando?
1.  Compila la aplicación web optimizada usando Vite en la carpeta `dist`.
2.  Llama a `electron-builder` para empaquetar el código compilado junto al runtime de Electron.
3.  Genera el archivo ejecutable en una nueva carpeta llamada `dist-desktop/`. 

*Tu archivo listo para distribuir estará ubicado en: `dist-desktop/SculptEngine [Versión].exe`.*

---

## 📱 Paso 3: Generar el Archivo .APK (Android Móvil)

Para móviles, utilizamos **Capacitor**, la tecnología moderna que envuelve la app web en un contenedor nativo optimizado con acceso completo a APIs del sistema y hardware.

Sigue estos pasos en tu terminal:

### 1. Inicializar la plataforma Android
Solo debes ejecutar esto la primera vez para crear la estructura del proyecto Android nativo:
```bash
npm run cap:add:android
```

### 2. Sincronizar el código
Cada vez que hagas cambios en el código de la web y quieras probarlos en el móvil, ejecuta:
```bash
npm run cap:sync
```
*Este comando compila el proyecto con Vite y copia los assets a la carpeta del proyecto Android nativo.*

### 3. Abrir en Android Studio para Compilar el APK
Para abrir el entorno de desarrollo móvil y compilar el archivo `.apk` definitivo:
```bash
npm run cap:android
```
*Esto abrirá automáticamente Android Studio apuntando a tu proyecto.*

### 4. Generar el APK en Android Studio:
Una vez que cargue Android Studio (espera a que terminen de indexar los procesos del gradle en la barra inferior):
1. En el menú superior, ve a: **Build** > **Build Bundle(s) / APK(s)** > **Build APK(s)**.
2. Android Studio compilará tu aplicación en unos segundos.
3. Al terminar, verás un globo de notificación en la esquina inferior derecha con un enlace de **"locate"**.
4. Haz clic en **locate** para abrir el explorador de archivos directamente en la carpeta donde se encuentra tu archivo `app-debug.apk` listo para ser instalado en cualquier teléfono Android.

---

## 🛠️ Probando la App en Desarrollo
Si deseas correr la app en modo ventana de escritorio para probarla antes de compilar:
```bash
npm run app:dev
```
Si deseas probar la app móvil conectando tu teléfono por USB:
1. Activa la "Depuración por USB" en tu celular (en Opciones de desarrollador).
2. Conecta el teléfono a la PC.
3. En Android Studio, selecciona tu celular en la lista de dispositivos (arriba a la derecha) y haz clic en el botón verde de **Play / Run**. La app se instalará e iniciará en tu teléfono automáticamente en tiempo real.
