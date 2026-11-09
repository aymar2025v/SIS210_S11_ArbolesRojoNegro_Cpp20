"""
demo_secuencias.py - Actividad 1 en Python: secuencias A y B con trazas paso a paso.
Ejecutar:  python demo_secuencias.py
Genera arbol_A_py.dot y arbol_B_py.dot (diagramas Graphviz) para el informe.
"""
from rn import ArbolRN


def describir(eventos: list) -> str:
    if not eventos:
        return "- (sin fixup)"
    return " + ".join(c if r == "-" else f"{c} [{r}]" for c, r in eventos)


print("==== Secuencia A: 10, 20, 30, 15, 25 ====")
a = ArbolRN()
a.registrar = True
for k in (10, 20, 30, 15, 25):
    a.insertar(k, k)
    print(f"\n-- insertar {k} --> {describir(a.eventos)}")
    print(a.a_texto())
    assert a.es_valido()
print(f"\nAltura = {a.altura()}, bh(raiz) = {a.altura_negra()}")
open("arbol_A_py.dot", "w", encoding="utf-8").write(a.a_dot(incluir_nil=True))

print("\n==== Secuencia B: 7, 3, 18, 10, 22, 8, 11, 26 ====")
b = ArbolRN()
b.registrar = True
print("Paso | Clave | Caso fixup / rotacion                    | Estado raiz")
print("-----+-------+------------------------------------------+------------")
for paso, k in enumerate((7, 3, 18, 10, 22, 8, 11, 26), start=1):
    b.insertar(k, k)
    print(f"{paso:^5}| {k:^6}| {describir(b.eventos):<41}| {b.raiz}")
    assert b.es_valido()
print("\nArbol final:")
print(b.a_texto())
print(f"rotaciones totales = {b.rotaciones}  (la Secuencia B solo usa el Caso 1)")
open("arbol_B_py.dot", "w", encoding="utf-8").write(b.a_dot(incluir_nil=True))
print("\n(diagramas escritos: arbol_A_py.dot, arbol_B_py.dot)")
