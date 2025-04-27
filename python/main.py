import sys
import glob
import threading
import time
import serial
import pyautogui
import tkinter as tk
from tkinter import ttk, messagebox

# Configurações PyAutoGUI
pyautogui.PAUSE = 0
pyautogui.FAILSAFE = False

# Parâmetros de joystick
alpha = 0.2       # suavização exponencial (0.1 - 0.3)
sensitivity = 0.05 # sensibilidade (0.0 - 1.0; <1 reduz movimento)
smoothed = {0: 0.0, 1: 0.0}
keys_pressed = set()

# Buffer de bytes serial
data_buffer = bytearray()

# Move o mouse aplicando filtro exponencial e sensibilidade
def move_mouse(axis, raw_value):
    if axis == 2:
            pyautogui.keyDown('space')
            print("PRESSIONADO: SPACE")
            time.sleep(0.1)
            pyautogui.keyUp('space')
            time.sleep(0.1)

    else:
        # filtro exponencial
        s = smoothed[axis] + alpha * (raw_value - smoothed[axis])
        smoothed[axis] = s
        # aplica sensibilidade
        delta = int(round(s * sensitivity))
        if delta:
            if axis == 0:
                pyautogui.moveRel(delta, 0, duration=0)
            if axis == 1:
                pyautogui.moveRel(0, delta, duration=0)
        


# Interpreta 3 bytes de joystick
def parse_data(packet):
    axis = packet[0]
    value = int.from_bytes(packet[1:3], byteorder='little', signed=True)
    return axis, value

# Loop unificado de leitura serial
def controle(ser):
    txt_buf = ''
    RELEASE_DELAY = 0.2
    last_time = {}
    ser.timeout = 0
    while True:
        # lê todos bytes disponíveis
        n = ser.in_waiting or 1
        chunk = ser.read(n)
        now = time.time()
        data_buffer.extend(chunk)
        # pacotes de joystick (0xFF + 3 bytes)
        while len(data_buffer) >= 4:
            if data_buffer[0] != 0xFF:
                idx = data_buffer.find(0xFF)
                if idx < 0:
                    data_buffer.clear()
                    break
                del data_buffer[:idx]
                if len(data_buffer) < 4:
                    break
            packet = data_buffer[1:4]
            axis, val = parse_data(packet)
            move_mouse(axis, val)
            print(f"MOUSE: eixo={axis}, val={val}")
            del data_buffer[:4]
        # comandos ASCII para teclas 'q','w','a','s','d'

        try:
            txt_buf += chunk.decode('ascii')
        except UnicodeDecodeError:
            pass

        if '\n' in txt_buf:
            lines = txt_buf.split('\n')
            for line in lines[:-1]:
                # ignora simples 'q','w','a','s','d'
                if line in ('q','w','a','s','d', "CLICK", "click"):
                    continue
                # só processa linhas que contenham pelo menos um ':'
                if ':' in line:
                    parts = line.strip().split(':', 1)
                    if len(parts) != 2:
                        # linha inválida, pula
                        continue
                    key, action = parts
                    key = key.lower()
                    
                    if key == "click":
                        pyautogui.click()
                    if action == 'DOWN' and key not in keys_pressed:
                        pyautogui.keyDown(key)
                        keys_pressed.add(key)
                        print(f"PRESSIONADO: {key}")
                    elif action == 'UP' and key in keys_pressed:
                        pyautogui.keyUp(key)
                        keys_pressed.remove(key)
                        print(f"SOLTOU: {key}")
            # mantém apenas o resto incompleto no buffer
            txt_buf = lines[-1]


# Retorna portas seriais disponíveis
def serial_ports():
    ports = []
    if sys.platform.startswith('win'):
        candidates = [f'COM{i}' for i in range(1, 33)]
    elif sys.platform.startswith(('linux','cygwin')):
        candidates = glob.glob('/dev/ttyA*')
    elif sys.platform.startswith('darwin'):
        candidates = glob.glob('/dev/tty.*')
    else:
        raise EnvironmentError('Plataforma não suportada')
    for p in candidates:
        try:
            s = serial.Serial(p, 115200, timeout=0)
            s.close()
            ports.append(p)
        except:
            pass
    return ports

# Conecta à porta e inicia leitura
def conectar_porta(port_name, root, botao_conectar, status_label, mudar_cor_circulo):
    if not port_name:
        messagebox.showwarning("Aviso", "Selecione uma porta serial antes de conectar.")
        return
    try:
        ser = serial.Serial(port_name, 115200, timeout=0)
        status_label.config(text=f"Conectado em {port_name}", foreground="green")
        mudar_cor_circulo("green")
        botao_conectar.config(text="Conectado")
        threading.Thread(target=controle, args=(ser,), daemon=True).start()
    except Exception as e:
        messagebox.showerror("Erro de Conexão", f"Não foi possível conectar em {port_name}.\nErro: {e}")
        mudar_cor_circulo("red")

# Interface gráfica
def criar_janela():
    root = tk.Tk()
    root.title("Controle de Mouse e Teclado")
    root.geometry("400x250")
    root.config(bg="#2e2e2e")

    style = ttk.Style(root)
    style.theme_use("clam")
    style.configure("TFrame", background="#2e2e2e")
    style.configure("TLabel", background="#2e2e2e", foreground="#ffffff")
    style.configure("TButton", background="#444444", foreground="#ffffff")
    style.configure("Accent.TButton", background="#007acc", foreground="#ffffff")

    frame = ttk.Frame(root, padding=20)
    frame.pack(expand=True, fill="both")

    ttk.Label(frame, text="Controle de Mouse e Teclado", font=("Segoe UI", 14, "bold")).pack(pady=(0,10))

    porta_var = tk.StringVar()
    portas = serial_ports()
    if portas:
        porta_var.set(portas[0])
    combobox = ttk.Combobox(frame, textvariable=porta_var, values=portas, state="readonly", width=15)
    combobox.pack(pady=5)

    botao_conectar = ttk.Button(frame, text="Conectar e Iniciar", style="Accent.TButton",
                                command=lambda: conectar_porta(porta_var.get(), root, botao_conectar, status_label, mudar_cor_circulo))
    botao_conectar.pack(pady=10)

    status_label = tk.Label(frame, text="Aguardando seleção de porta...", bg="#2e2e2e", fg="#ffffff")
    status_label.pack()

    circle_canvas = tk.Canvas(frame, width=20, height=20, bg="#2e2e2e", highlightthickness=0)
    circle_item = circle_canvas.create_oval(2,2,18,18, fill="red")
    circle_canvas.pack(pady=10)

    def mudar_cor_circulo(cor):
        circle_canvas.itemconfig(circle_item, fill=cor)

    root.mainloop()

if __name__ == "__main__":
    criar_janela()
