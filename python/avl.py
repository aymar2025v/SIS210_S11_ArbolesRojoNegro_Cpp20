"""avl.py - AVL minimo para comparar alturas (en tu practica usa tu implementacion de S10;
esta existe para que el experimento corra solo). La altura guardada en cada nodo es en aristas."""
from __future__ import annotations


class _N:
    __slots__ = ("k", "v", "izq", "der", "h")

    def __init__(self, k, v):
        self.k, self.v, self.izq, self.der, self.h = k, v, None, None, 0


def _h(n) -> int:
    return -1 if n is None else n.h


def _act(n) -> None:
    n.h = 1 + max(_h(n.izq), _h(n.der))


def _rot_der(y):
    x = y.izq
    y.izq, x.der = x.der, y
    _act(y)
    _act(x)
    return x


def _rot_izq(x):
    y = x.der
    x.der, y.izq = y.izq, x
    _act(x)
    _act(y)
    return y


class AVL:
    def __init__(self) -> None:
        self.raiz = None

    def insertar(self, k, v=None) -> None:
        self.raiz = self._ins(self.raiz, k, v)   # recursion de profundidad <= 1.44*log2(n)

    def _ins(self, n, k, v):
        if n is None:
            return _N(k, v)
        if k < n.k:
            n.izq = self._ins(n.izq, k, v)
        elif n.k < k:
            n.der = self._ins(n.der, k, v)
        else:
            n.v = v
            return n
        _act(n)
        balance = _h(n.izq) - _h(n.der)
        if balance > 1:                                   # pesado a la izquierda
            if _h(n.izq.izq) < _h(n.izq.der):
                n.izq = _rot_izq(n.izq)                   # caso izquierda-derecha
            return _rot_der(n)
        if balance < -1:                                  # pesado a la derecha
            if _h(n.der.der) < _h(n.der.izq):
                n.der = _rot_der(n.der)                   # caso derecha-izquierda
            return _rot_izq(n)
        return n

    def altura(self) -> int:
        return _h(self.raiz)
