from machine import Pin, SPI, ADC
import utime
from ili9488 import ILI9488

# ======================
# CONFIGURACIÓ HARDWARE
# ======================
spi = SPI(2, baudrate=40000000, polarity=0, phase=0)
cs = Pin(15, Pin.OUT)
dc = Pin(2, Pin.OUT)
rst = Pin(4, Pin.OUT)

tft = ILI9488(spi, cs=cs, dc=dc, rst=rst)
tft.fill_color(0x0000)  # fons negre

# ======================
# CONFIGURACIÓ SENSORS
# ======================
pos_adc = ADC(Pin(34))
pres_adc = ADC(Pin(35))
car_adc = ADC(Pin(32))

pos_adc.atten(ADC.ATTN_11DB)
pres_adc.atten(ADC.ATTN_11DB)
car_adc.atten(ADC.ATTN_11DB)

POS_MAX = 200.0     # mm
PRES_MAX = 250.0    # bar
CAR_MAX = 10000.0   # N

# ======================
# BUFFER DE DADES
# ======================
MAX_POINTS = 600
posicions = [0] * MAX_POINTS
pressions = [0] * MAX_POINTS
timestamps = [0] * MAX_POINTS
idx = 0

# ======================
# FUNCIONS DE LECTURA
# ======================
def llegir_pos():
    val = pos_adc.read()
    return (val / 4095.0) * POS_MAX

def llegir_pres():
    val = pres_adc.read()
    return (val / 4095.0) * PRES_MAX

def llegir_car():
    val = car_adc.read()
    return (val / 4095.0) * CAR_MAX

# ======================
# FUNCIONS DE VISUALITZACIÓ
# ======================
def mostrar_header():
    tft.text("Cilindre H1", 20, 10, color=0xFFE0, size=3)  # groc

def mostrar_dades(pos, pres, car):
    # zona inferior
    tft.fill_rect(0, 270, 480, 50, 0x0000)
    tft.text(f"Pos: {pos:5.1f} mm", 10, 280, color=0x07FF, size=2)
    tft.text(f"Pres: {pres:5.1f} bar", 170, 280, color=0xF800, size=2)
    tft.text(f"Car: {car:6.0f} N", 330, 280, color=0x07E0, size=2)

def dibuixar_grafic():
    GRAPH_X, GRAPH_Y, GRAPH_W, GRAPH_H = 40, 80, 400, 140
    tft.fill_rect(GRAPH_X, GRAPH_Y, GRAPH_W, GRAPH_H, 0x0000)
    tft.rect(GRAPH_X-1, GRAPH_Y-1, GRAPH_W+2, GRAPH_H+2, 0xFFFF)

    now = utime.ticks_ms()
    window = 10 * 60 * 1000  # 10 minuts en ms

    for i in range(1, MAX_POINTS):
        idx1 = (idx - i) % MAX_POINTS
        idx2 = (idx - i - 1) % MAX_POINTS
        t1, t2 = timestamps[idx1], timestamps[idx2]
        if t1 == 0 or now - t1 > window:
            break

        # Escalat
        x1 = GRAPH_X + GRAPH_W - int((now - t1) / window * GRAPH_W)
        x2 = GRAPH_X + GRAPH_W - int((now - t2) / window * GRAPH_W)

        y1_pos = GRAPH_Y + GRAPH_H - int(posicions[idx1] / POS_MAX * GRAPH_H)
        y2_pos = GRAPH_Y + GRAPH_H - int(posicions[idx2] / POS_MAX * GRAPH_H)

        y1_pres = GRAPH_Y + GRAPH_H - int(pressions[idx1] / PRES_MAX * GRAPH_H)
        y2_pres = GRAPH_Y + GRAPH_H - int(pressions[idx2] / PRES_MAX * GRAPH_H)

        # Dibuixa línies
        tft.line(x1, y1_pos, x2, y2_pos, 0x001F)  # blau: posició
        tft.line(x1, y1_pres, x2, y2_pres, 0xF800)  # vermell: pressió

    # llegenda
    tft.text("Posicio", 40, GRAPH_Y - 20, color=0x001F, size=2)
    tft.text("Pressio", 150, GRAPH_Y - 20, color=0xF800, size=2)

# ======================
# BUCLE PRINCIPAL
# ======================
mostrar_header()
t_actual = utime.ticks_ms()
last_update = t_actual
last_graph = t_actual

while True:
    t_actual = utime.ticks_ms()

    # Actualitza lectures cada segon
    if utime.ticks_diff(t_actual, last_update) > 1000:
        last_update = t_actual

        pos = llegir_pos()
        pres = llegir_pres()
        car = llegir_car()

        posicions[idx] = pos
        pressions[idx] = pres
        timestamps[idx] = t_actual
        idx = (idx + 1) % MAX_POINTS

        mostrar_dades(pos, pres, car)

    # Actualitza gràfic cada 5 segons
    if utime.ticks_diff(t_actual, last_graph) > 5000:
        last_graph = t_actual
        dibuixar_grafic()
