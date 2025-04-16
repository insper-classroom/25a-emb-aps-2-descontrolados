import serial
import pyautogui

porta = serial.Serial('COM5', 115200)  # troque COMx pela porta certa (ex: COM3 ou /dev/ttyACM0)

teclas_pressionadas = set()

while True:
    linha = porta.readline().decode().strip()
    if ':' in linha:
        tecla, acao = linha.split(':')
        tecla = tecla.lower()  # pyautogui usa letras minúsculas

        if acao == 'DOWN' and tecla not in teclas_pressionadas:
            pyautogui.keyDown(tecla)
            teclas_pressionadas.add(tecla)
            print(f"Tecla pressionada: {tecla}")
        elif acao == 'UP' and tecla in teclas_pressionadas:
            pyautogui.keyUp(tecla)
            teclas_pressionadas.remove(tecla)
            print(f"Tecla solta: {tecla}")
