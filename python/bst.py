"""bst.py - BST simple SIN balanceo, solo para medir alturas (Actividad 3).
Insercion y altura ITERATIVAS: con claves ordenadas el arbol degenera en una lista de n nodos
y una version recursiva superaria el limite de recursion de Python."""
from __future__ import annotations


class _N:
    __slots__ = ("k", "izq", "der")

    def __init__(self, k):
        self.k, self.izq, self.der = k, None, None


class BST:
    def __init__(self) -> None:
        self.raiz = None

    def insertar(self, k) -> None:
        nuevo = _N(k)
        if self.raiz is None:
            self.raiz = nuevo
            return
        n = self.raiz
        while True:
            if k < n.k:
                if n.izq is None:
                    n.izq = nuevo
                    return
                n = n.izq
            else:
                if n.der is None:
                    n.der = nuevo
                    return
                n = n.der

    def altura(self) -> int:
        """Altura en aristas (vacio = -1), recorrido por niveles."""
        if self.raiz is None:
            return -1
        nivel, h = [self.raiz], -1
        while nivel:
            h += 1
            nivel = [c for n in nivel for c in (n.izq, n.der) if c is not None]
        return h
