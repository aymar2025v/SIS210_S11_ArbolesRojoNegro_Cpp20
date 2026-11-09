"""
experimento.py - Actividad 3: altura de BST vs AVL vs Rojo-Negro (Python 3.12)

Inserta n claves en tres escenarios (aleatorio, creciente, decreciente) y mide la altura
(en ARISTAS: un solo nodo = 0). Compara con las cotas teoricas de la guia:
    AVL <= 1.44 * log2(n+2)        RN <= 2 * log2(n+1)
Uso:
    python experimento.py            # n = 100, 500, 1000, 5000, 10000  (tarda unos segundos)
    python experimento.py --rapido   # n = 100, 500, 1000
"""
import math
import random
import sys
import time

from avl import AVL
from bst import BST
from rn import ArbolRN


def experimento(n: int, filas: list) -> None:
    random.seed(2026)                                  # resultados reproducibles
    claves_rnd = random.sample(range(1, n * 10), n)
    claves_asc = list(range(1, n + 1))
    claves_desc = list(range(n, 0, -1))
    teorico_avl = round(1.44 * math.log2(n + 2), 1)
    teorico_rn = round(2 * math.log2(n + 1), 1)

    for escenario, claves in [("aleatorio", claves_rnd), ("creciente", claves_asc), ("decreciente", claves_desc)]:
        bst, avl, rn = BST(), AVL(), ArbolRN()
        t0 = time.perf_counter()
        for k in claves:
            bst.insertar(k)
            avl.insertar(k, k)
            rn.insertar(k, k)
        dt = time.perf_counter() - t0
        assert rn.es_valido(), "el arbol RN dejo de ser valido"
        hb, ha, hr = bst.altura(), avl.altura(), rn.altura()
        print(f"n={n:6d} {escenario:12s} | BST={hb:5d} AVL={ha:3d} RN={hr:3d} | "
              f"teorico AVL<={teorico_avl} RN<={teorico_rn} | rotaciones RN={rn.rotaciones:6d} | {dt:5.1f}s")
        filas.append((n, escenario, hb, ha, hr, teorico_avl, teorico_rn, rn.rotaciones))


if __name__ == "__main__":
    tamanos = [100, 500, 1000] if "--rapido" in sys.argv else [100, 500, 1000, 5000, 10000]
    filas: list = []
    print("Alturas en aristas (1 nodo = 0).  n  escenario    | BST  AVL  RN  | cotas teoricas")
    print("-" * 110)
    for n in tamanos:
        experimento(n, filas)

    print("\nTabla para el informe (formato de la guia):")
    print(f"{'n':>6} | {'Escenario':<11} | {'BST':>5} | {'AVL':>4} | {'RN':>4} | {'Cota AVL':>8} | {'Cota RN':>7} | Observacion")
    print("-" * 100)
    for n, esc, hb, ha, hr, ca, cr, _ in filas:
        obs = []
        if hb >= n - 1:
            obs.append("BST degenerado (h = n-1)")
        obs.append("AVL ok" if ha <= ca else "AVL EXCEDE")
        obs.append("RN ok" if hr <= cr else "RN EXCEDE")
        print(f"{n:6d} | {esc:<11} | {hb:5d} | {ha:4d} | {hr:4d} | {ca:8.1f} | {cr:7.1f} | {', '.join(obs)}")
