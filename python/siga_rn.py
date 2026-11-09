"""
siga_rn.py - Actividad 4: SIGA-RN, estudiantes UNAP en un arbol Rojo-Negro (Python 3.12)
Ejecutar:  python siga_rn.py
"""
import statistics
import time
from dataclasses import dataclass

from rn import ArbolRN   # tu implementacion (rn.py)


@dataclass
class EstudianteUNAP:
    codigo: str           # PU-XXXXXXXX
    nombres: str
    escuela: str
    ciclo: int
    promedio: float


class SIGA_RN:
    def __init__(self) -> None:
        self.arbol: ArbolRN[str, EstudianteUNAP] = ArbolRN()

    def matricular(self, est: EstudianteUNAP) -> None:
        self.arbol.insertar(est.codigo, est)

    def buscar(self, codigo: str, repeticiones: int = 2000):
        """Busca y mide: UNA busqueda (ruidosa) y la MEDIANA de muchas (mas confiable)."""
        t0 = time.perf_counter_ns()
        nodo = self.arbol.buscar(codigo)
        t1 = time.perf_counter_ns()
        muestras = []
        for _ in range(repeticiones):
            a = time.perf_counter_ns()
            self.arbol.buscar(codigo)
            muestras.append(time.perf_counter_ns() - a)
        print(f"Busqueda '{codigo}': {t1 - t0} ns (1 vez) | mediana de {repeticiones} = "
              f"{statistics.median(muestras):.0f} ns | altura arbol = {self.arbol.altura()}")
        return nodo.valor if nodo else None

    def listar_ordenado(self) -> list:
        """Recorrido inorden -> lista ordenada por codigo."""
        resultado: list = []
        self.arbol.inorden(lambda k, v: resultado.append(v))
        return resultado


if __name__ == "__main__":
    siga = SIGA_RN()
    estudiantes = [
        EstudianteUNAP("PU-20200101", "Mamani Quispe, J.", "Sistemas", 5, 14.2),
        EstudianteUNAP("PU-20200387", "Huanca Apaza, L.", "Sistemas", 5, 13.8),
        EstudianteUNAP("PU-20210045", "Ccopa Flores, M.", "IACD", 3, 15.1),
        EstudianteUNAP("PU-20210312", "Ticona Condori, R.", "Sistemas", 3, 12.9),
        EstudianteUNAP("PU-20220089", "Cutipa Larico, A.", "IACD", 1, 16.0),
        EstudianteUNAP("PU-20220154", "Pari Calisaya, Y.", "Sistemas", 1, 11.5),
        EstudianteUNAP("PU-20180023", "Coyla Mamani, H.", "Sistemas", 9, 15.7),
        EstudianteUNAP("PU-20190278", "Quispe Tito, B.", "IACD", 7, 14.8),
        EstudianteUNAP("PU-20230012", "Apaza Cruz, N.", "Sistemas", 1, 13.3),
        EstudianteUNAP("PU-20230198", "Larico Huanca, C.", "IACD", 1, 12.1),
    ]
    for e in estudiantes:
        siga.matricular(e)

    siga.buscar("PU-20200387")
    siga.buscar("PU-20230198")
    if siga.buscar("PU-99999999") is None:
        print("(PU-99999999 no esta matriculado)")

    print("\nArbol resultante:")
    print(siga.arbol.a_texto())
    print(f"tam = {len(siga.arbol)}, altura = {siga.arbol.altura()}, "
          f"bh(raiz) = {siga.arbol.altura_negra()}, valido = {siga.arbol.es_valido()}")

    print("\nLista ordenada:")
    for e in siga.listar_ordenado():
        print(f"  {e.codigo} | {e.nombres} | ciclo {e.ciclo}")

    with open("arbol_siga_py.dot", "w", encoding="utf-8") as f:
        f.write(siga.arbol.a_dot(incluir_nil=True))
    print("\n(diagrama Graphviz escrito en arbol_siga_py.dot)")
