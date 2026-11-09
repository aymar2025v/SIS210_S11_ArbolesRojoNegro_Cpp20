"""
rn.py - Arbol Rojo-Negro en Python 3.12
SIS210 - Algoritmos y Estructuras de Datos | UNAP 2026-II | Semana 11

Mismas ideas y mismos nombres que la version de C++ (rn.h):
  * 5 propiedades: P1 color, P2 raiz negra, P3 NIL negro, P4 sin rojos seguidos,
    P5 misma altura negra en todos los caminos.
  * insertar con fixup (Casos 1, 2, 3 y sus simetricos), eliminar con fixup (Casos 1-4).
  * Todo es ITERATIVO (salvo utilidades de impresion/verificacion): un BST degenerado de
    10 000 nodos reventaria el limite de recursion de Python (1000), un RN jamas.

Uso rapido:
    from rn import ArbolRN
    a = ArbolRN()
    for k in (10, 20, 30, 15, 25):
        a.insertar(k, k)
    print(a.a_texto())
"""
from __future__ import annotations

from enum import Enum
from typing import Callable, Generic, Iterator, Optional, TypeVar

K = TypeVar("K")
V = TypeVar("V")


class Color(Enum):
    ROJO = "R"
    NEGRO = "N"


class Nodo(Generic[K, V]):
    """Nodo del arbol. __slots__ evita el dict por instancia (menos memoria, acceso mas rapido)."""

    __slots__ = ("clave", "valor", "color", "padre", "izq", "der")

    def __init__(self, clave: K, valor: V) -> None:
        self.clave = clave
        self.valor = valor
        self.color = Color.ROJO          # todo nodo nuevo nace ROJO (no altera P5)
        self.padre: Optional[Nodo[K, V]] = None
        self.izq: Optional[Nodo[K, V]] = None
        self.der: Optional[Nodo[K, V]] = None

    def __repr__(self) -> str:
        return f"{self.clave!r}({self.color.value})"


def _es_rojo(n: Optional[Nodo]) -> bool:
    """P3 hecha codigo: None (NIL) cuenta como NEGRO."""
    return n is not None and n.color is Color.ROJO


class ArbolRN(Generic[K, V]):
    """Arbol Rojo-Negro generico. Las claves deben ser comparables con '<'."""

    def __init__(self) -> None:
        self._raiz: Optional[Nodo[K, V]] = None
        self._tam = 0
        self.rotaciones = 0                       # estadistica acumulada
        self.registrar = False                    # si True, guarda los casos de cada operacion
        self.eventos: list[tuple[str, str]] = []  # [(caso, rotacion)] de la ULTIMA operacion

    # ------------------------------------------------------------------ consultas
    @property
    def raiz(self) -> Optional[Nodo[K, V]]:
        return self._raiz

    def __len__(self) -> int:
        return self._tam

    def __contains__(self, clave: K) -> bool:
        return self.buscar(clave) is not None

    def buscar(self, clave: K) -> Optional[Nodo[K, V]]:
        """Busqueda BST iterativa. None si no existe."""
        n = self._raiz
        while n is not None:
            if clave < n.clave:
                n = n.izq
            elif n.clave < clave:
                n = n.der
            else:
                return n
        return None

    def minimo(self) -> Optional[Nodo[K, V]]:
        return None if self._raiz is None else self._minimo_desde(self._raiz)

    def maximo(self) -> Optional[Nodo[K, V]]:
        n = self._raiz
        while n is not None and n.der is not None:
            n = n.der
        return n

    @staticmethod
    def _minimo_desde(n: Nodo[K, V]) -> Nodo[K, V]:
        while n.izq is not None:
            n = n.izq
        return n

    def altura(self) -> int:
        """Altura en ARISTAS: vacio = -1, un nodo = 0. Recorre por niveles (sin recursion)."""
        if self._raiz is None:
            return -1
        nivel, h = [self._raiz], -1
        while nivel:
            h += 1
            nivel = [h_ for n in nivel for h_ in (n.izq, n.der) if h_ is not None]
        return h

    def altura_negra(self, n: Optional[Nodo[K, V]] = None) -> int:
        """bh(n): nodos negros de n a un NIL, SIN contar n y CONTANDO el NIL.
        Devuelve -1 si el subarbol es inconsistente (P5 violada). Por defecto, desde la raiz."""
        if n is None:
            n = self._raiz
        return self._bh(n)

    def _bh(self, n: Optional[Nodo[K, V]]) -> int:
        if n is None:
            return 0
        hi, hd = self._bh(n.izq), self._bh(n.der)   # recursion de profundidad <= 2*log2(n)
        if hi < 0 or hd < 0:
            return -1
        por_izq = hi + (0 if _es_rojo(n.izq) else 1)
        por_der = hd + (0 if _es_rojo(n.der) else 1)
        return por_izq if por_izq == por_der else -1

    # ------------------------------------------------------------------ recorridos
    def __iter__(self) -> Iterator[tuple[K, V]]:
        """Inorden ITERATIVO con pila explicita: genera (clave, valor) en orden ascendente."""
        pila: list[Nodo[K, V]] = []
        n = self._raiz
        while pila or n is not None:
            while n is not None:
                pila.append(n)
                n = n.izq
            n = pila.pop()
            yield n.clave, n.valor
            n = n.der

    def inorden(self, visita: Callable[[K, V], None]) -> None:
        """Aplica visita(clave, valor) en orden ascendente (la API que usa la guia)."""
        for k, v in self:
            visita(k, v)

    # ------------------------------------------------------------------ rotaciones
    def _rotar_izquierda(self, x: Nodo[K, V]) -> None:
        """x->der sube, x baja a su izquierda.    x            y
                                                 / \\          / \\
                                                A   y   ==>  x   C
                                                   / \\      / \\
                                                  B   C    A   B"""
        y = x.der
        assert y is not None
        x.der = y.izq                      # el subarbol B pasa a ser hijo derecho de x
        if y.izq is not None:
            y.izq.padre = x
        y.padre = x.padre                  # y hereda el padre de x
        if x.padre is None:
            self._raiz = y
        elif x is x.padre.izq:
            x.padre.izq = y
        else:
            x.padre.der = y
        y.izq = x                          # x pasa a ser hijo izquierdo de y
        x.padre = y
        self.rotaciones += 1

    def _rotar_derecha(self, x: Nodo[K, V]) -> None:
        """x->izq sube, x baja a su derecha (espejo de _rotar_izquierda)."""
        y = x.izq
        assert y is not None
        x.izq = y.der
        if y.der is not None:
            y.der.padre = x
        y.padre = x.padre
        if x.padre is None:
            self._raiz = y
        elif x is x.padre.der:
            x.padre.der = y
        else:
            x.padre.izq = y
        y.der = x
        x.padre = y
        self.rotaciones += 1

    def _rotar(self, x: Nodo[K, V], a_la_izquierda: bool, caso: str) -> None:
        """Rota y deja constancia del caso (para la tabla de la guia)."""
        if a_la_izquierda:
            self._rotar_izquierda(x)
        else:
            self._rotar_derecha(x)
        self._anotar(caso, "rotar_izquierda" if a_la_izquierda else "rotar_derecha")

    def _anotar(self, caso: str, rotacion: str = "-") -> None:
        if self.registrar:
            self.eventos.append((caso, rotacion))

    # ------------------------------------------------------------------ insercion
    def insertar(self, clave: K, valor: V) -> bool:
        """Inserta (clave, valor). Si la clave existe, solo actualiza el valor.
        Devuelve True si creo un nodo nuevo, False si actualizo."""
        if self.registrar:
            self.eventos.clear()
        padre: Optional[Nodo[K, V]] = None
        n = self._raiz
        while n is not None:                       # (a) descenso BST
            padre = n
            if clave < n.clave:
                n = n.izq
            elif n.clave < clave:
                n = n.der
            else:
                n.valor = valor
                return False
        z = Nodo(clave, valor)                     # (b) crear (ROJO) y enlazar
        z.padre = padre
        if padre is None:
            self._raiz = z
        elif clave < padre.clave:
            padre.izq = z
        else:
            padre.der = z
        self._tam += 1
        self._insertar_fixup(z)                    # (c) reparar propiedades
        return True

    def _insertar_fixup(self, z: Nodo[K, V]) -> None:
        """Restaura P2/P4 tras insertar z (ROJO). Casos 1, 2, 3 y sus simetricos.
        p = padre, g = abuelo, tio = hermano de p. Si p es rojo, g existe (la raiz es negra)."""
        while _es_rojo(z.padre):
            p = z.padre
            g = p.padre
            padre_es_izq = g.izq is p
            tio = g.der if padre_es_izq else g.izq
            if _es_rojo(tio):
                # CASO 1: tio rojo -> recolorear y subir
                p.color = Color.NEGRO
                tio.color = Color.NEGRO
                g.color = Color.ROJO
                self._anotar("Caso 1 (tio rojo: recolorear)")
                z = g
                continue
            # tio negro: hay que rotar
            z_es_interior = (z is p.der) if padre_es_izq else (z is p.izq)
            if z_es_interior:
                # CASO 2 (triangulo): rotar en p para pasar al Caso 3
                z = p
                self._rotar(z, padre_es_izq, "Caso 2 (triangulo)")
                p = z.padre
            # CASO 3 (linea): p negro, g rojo, rotar en g hacia el lado contrario
            p.color = Color.NEGRO
            g.color = Color.ROJO
            self._rotar(g, not padre_es_izq, "Caso 3 (linea)")
        self._raiz.color = Color.NEGRO              # P2

    # ------------------------------------------------------------------ eliminacion
    def eliminar(self, clave: K) -> bool:
        """Elimina la clave. Devuelve True si existia."""
        z = self.buscar(clave)
        if z is None:
            return False
        if self.registrar:
            self.eventos.clear()
        color_perdido = z.color
        if z.izq is None:                           # a lo sumo un hijo (derecho)
            x, padre_x = z.der, z.padre
            self._trasplantar(z, z.der)
        elif z.der is None:                         # un hijo (izquierdo)
            x, padre_x = z.izq, z.padre
            self._trasplantar(z, z.izq)
        else:                                       # dos hijos: entra el sucesor y
            y = self._minimo_desde(z.der)
            color_perdido = y.color
            x = y.der
            if y.padre is z:
                padre_x = y
            else:
                padre_x = y.padre
                self._trasplantar(y, y.der)
                y.der = z.der
                y.der.padre = y
            self._trasplantar(z, y)
            y.izq = z.izq
            y.izq.padre = y
            y.color = z.color
        self._tam -= 1
        if color_perdido is Color.NEGRO:
            self._eliminar_fixup(x, padre_x)
        return True

    def _trasplantar(self, u: Nodo[K, V], v: Optional[Nodo[K, V]]) -> None:
        """Reemplaza el subarbol de u por el de v en el padre de u."""
        if u.padre is None:
            self._raiz = v
        elif u is u.padre.izq:
            u.padre.izq = v
        else:
            u.padre.der = v
        if v is not None:
            v.padre = u.padre

    def _eliminar_fixup(self, x: Optional[Nodo[K, V]], padre_x: Optional[Nodo[K, V]]) -> None:
        """x (posible NIL) es 'doblemente negro'. w = hermano (nunca NIL por P5)."""
        while x is not self._raiz and not _es_rojo(x):
            x_es_izq = padre_x.izq is x
            w = padre_x.der if x_es_izq else padre_x.izq
            if _es_rojo(w):                         # CASO 1: hermano rojo
                w.color = Color.NEGRO
                padre_x.color = Color.ROJO
                self._rotar(padre_x, x_es_izq, "Elim. Caso 1 (hermano rojo)")
                w = padre_x.der if x_es_izq else padre_x.izq
            cercano = w.izq if x_es_izq else w.der
            lejano = w.der if x_es_izq else w.izq
            if not _es_rojo(cercano) and not _es_rojo(lejano):   # CASO 2: sobrinos negros
                w.color = Color.ROJO
                self._anotar("Elim. Caso 2 (sobrinos negros)")
                x, padre_x = padre_x, padre_x.padre
            else:
                if not _es_rojo(lejano):            # CASO 3: cercano rojo, lejano negro
                    cercano.color = Color.NEGRO
                    w.color = Color.ROJO
                    self._rotar(w, not x_es_izq, "Elim. Caso 3 (sobrino cercano rojo)")
                    w = padre_x.der if x_es_izq else padre_x.izq
                    lejano = w.der if x_es_izq else w.izq
                # CASO 4: lejano rojo -> rotar en el padre y terminar
                w.color = padre_x.color
                padre_x.color = Color.NEGRO
                lejano.color = Color.NEGRO
                self._rotar(padre_x, x_es_izq, "Elim. Caso 4 (sobrino lejano rojo)")
                x, padre_x = self._raiz, None
        if x is not None:
            x.color = Color.NEGRO

    # ------------------------------------------------------------------ verificacion
    def violacion(self) -> Optional[str]:
        """None si el arbol cumple BST + P2 + P4 + P5 + punteros padre + tamano;
        si no, describe la primera violacion."""
        if self._raiz is None:
            return None if self._tam == 0 else "tam != 0 en arbol vacio"
        if self._raiz.color is not Color.NEGRO:
            return "P2 violada: la raiz es roja"
        if self._raiz.padre is not None:
            return "la raiz tiene padre"
        contados = 0
        # pila de (nodo, cota_inf, cota_sup); bh se calcula aparte con altura_negra()
        pila = [(self._raiz, None, None)]
        while pila:
            n, inf, sup = pila.pop()
            contados += 1
            if inf is not None and not inf < n.clave:
                return "orden BST violado (clave demasiado chica)"
            if sup is not None and not n.clave < sup:
                return "orden BST violado (clave demasiado grande)"
            for hijo, i, s in ((n.izq, inf, n.clave), (n.der, n.clave, sup)):
                if hijo is not None:
                    if hijo.padre is not n:
                        return "un puntero 'padre' es inconsistente"
                    pila.append((hijo, i, s))
            if n.color is Color.ROJO and (_es_rojo(n.izq) or _es_rojo(n.der)):
                return "P4 violada: un nodo rojo tiene un hijo rojo"
        if contados != self._tam:
            return "el contador de tamano no coincide con los nodos reales"
        if self.altura_negra() < 0:
            return "P5 violada: caminos con distinta cantidad de nodos negros"
        return None

    def es_valido(self) -> bool:
        return self.violacion() is None

    # ------------------------------------------------------------------ impresion
    def a_texto(self) -> str:
        """Dibuja el arbol en texto ('.' = hueco NIL, solo cuando distingue izquierda/derecha)."""
        if self._raiz is None:
            return "(arbol vacio)"
        lineas: list[str] = []

        def rec(n: Optional[Nodo[K, V]], prefijo: str, rama: str) -> None:
            marca = "" if rama == "raiz" else ("+-- " if rama == "media" else "`-- ")
            if n is None:
                lineas.append(f"{prefijo}{marca}.")
                return
            lineas.append(f"{prefijo}{marca}{n.clave}({n.color.value})")
            if n.izq is None and n.der is None:
                return
            hijos = prefijo + ("" if rama == "raiz" else ("|   " if rama == "media" else "    "))
            rec(n.izq, hijos, "media")
            rec(n.der, hijos, "ultima")

        rec(self._raiz, "", "raiz")
        return "\n".join(lineas)

    def a_dot(self, incluir_nil: bool = False) -> str:
        """Formato Graphviz DOT para generar diagramas del informe."""
        salida = [
            "digraph ArbolRN {",
            '  node [shape=circle, style=filled, fontcolor=white, fontname="Helvetica", width=0.5];',
            "  edge [arrowhead=none];",
        ]
        contador = 1                                   # el id 0 es la raiz
        if self._raiz is not None:
            pila = [(self._raiz, 0)]                  # (nodo, su_id); los ids se asignan al ENCOLAR
            while pila:
                n, mi_id = pila.pop()
                relleno = "red" if n.color is Color.ROJO else "black"
                salida.append(f'  n{mi_id} [label="{n.clave}", fillcolor="{relleno}"];')
                for hijo in (n.izq, n.der):           # izquierda primero: asi Graphviz lo dibuja a la izquierda
                    if hijo is not None:
                        salida.append(f"  n{mi_id} -> n{contador};")
                        pila.append((hijo, contador))
                    elif incluir_nil:
                        salida.append(
                            f'  n{contador} [label="NIL", shape=box, fillcolor="black", fontsize=8, width=0.25, height=0.2];'
                        )
                        salida.append(f"  n{mi_id} -> n{contador};")
                    else:
                        continue
                    contador += 1
        salida.append("}")
        return "\n".join(salida) + "\n"
