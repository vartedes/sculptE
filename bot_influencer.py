import tweepy
import openai
import time
import random

# ==========================================
# 1. TUS CREDENCIALES (Reemplaza con tus datos)
# ==========================================

# Claves de la API de X (Twitter)
API_KEY = "TU_API_KEY"
API_SECRET = "TU_API_SECRET"
ACCESS_TOKEN = "TU_ACCESS_TOKEN"
ACCESS_TOKEN_SECRET = "TU_ACCESS_TOKEN_SECRET"

# Clave de OpenAI para la generación automática de contenido
OPENAI_API_KEY = "TU_OPENAI_API_KEY"

# Configuración de clientes
openai.api_key = OPENAI_API_KEY

client_x = tweepy.Client(
    consumer_key=API_KEY,
    consumer_secret=API_SECRET,
    access_token=ACCESS_TOKEN,
    access_token_secret=ACCESS_TOKEN_SECRET
)

# ==========================================
# 2. GENERADOR DE CONTENIDO CON IA
# ==========================================

LINK_DEMO = "https://www.youtube.com/watch?v=ZeqaQWYA_LY"

PROMPTS_TEMATICOS = [
    f"Escribe un tuit estilo influencer cripto experto y entusiasta sobre la importancia de leer el libro de órdenes (Order Book) en Binance para hacer scalping. Haz una mención sutil a la workstation Quantum Oracle V2 y añade este link: {LINK_DEMO}. Usa máximo 240 caracteres y 2 hashtags relevantes.",
    f"Escribe un tuit corto con un tip de trading avanzado para BTC/USDT sobre cómo evitar trampas de liquidez (Liquidity Sweeps). Recomienda monitorear la liquidez en tiempo real con Quantum Oracle V2 e incluye este link: {LINK_DEMO}. Máximo 240 caracteres.",
    f"Escribe un tuit promocional directo y persuasivo anunciando que solo quedan licencias 'Founder' vitalicias a $99 USDT para la terminal Quantum Oracle V2 antes de que pase a suscripción mensual. Incluye el link al demo real: {LINK_DEMO}. Usa emojis y hashtags.",
    f"Escribe un tuit analítico sobre la volatilidad actual del mercado cripto (BTC, ETH, SOL) y por qué ejecutar operaciones con baja latencia vía API es la ventaja de los traders profesionales. Menciona Quantum Oracle V2 y el link: {LINK_DEMO}."
]

def generar_tuit_ia():
    # Selecciona una temática al azar para que el contenido siempre varíe
    prompt_elegido = random.choice(PROMPTS_TEMATICOS)
    
    try:
        response = openai.chat.completions.create(
            model="gpt-3.5-turbo",
            messages=[
                {"role": "system", "content": "Eres un influencer cripto carismático, profesional y analítico especializado en trading cuantitativo y herramientas de alta frecuencia."},
                {"role": "user", "content": prompt_elegido}
            ],
            max_tokens=100,
            temperature=0.8
        )
        tuit_generado = response.choices[0].message.content.strip()
        # Limpiar comillas innecesarias si la IA las agrega
        tuit_generado = tuit_generado.replace('"', '')
        return tuit_generado
    except Exception as e:
        print(f"Error generando contenido con IA: {e}")
        # Tuit de respaldo en caso de fallo de API
        return f"Monitorea el libro de órdenes de Binance en tiempo real con Quantum Oracle V2 📈 Revisa la demo en vivo: {LINK_DEMO} #CryptoTrading #Binance"

# ==========================================
# 3. BUCLE DE PUBLICACIÓN AUTOMÁTICA
# ==========================================

def publicar_tuit():
    contenido = generar_tuit_ia()
    print(f"\n--- Generando nuevo tuit ---")
    print(contenido)
    
    try:
        client_x.create_tweet(text=contenido)
        print(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] ¡Tuit publicado exitosamente en @Qusdtcrypto!")
    except Exception as e:
        print(f"Error al publicar en X: {e}")

if __name__ == "__main__":
    print("🚀 Bot Influencer Cripto iniciado para @Qusdtcrypto...")
    
    while True:
        publicar_tuit()
        # Publica automáticamente cada 6 horas (21,600 segundos)
        print("Esperando 6 horas para la siguiente publicación...")
        time.sleep(21600)