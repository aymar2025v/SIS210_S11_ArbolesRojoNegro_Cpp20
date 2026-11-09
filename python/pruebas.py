"""
pruebas.py - Pruebas automaticas del arbol Rojo-Negro en Python.
Ejecutar:  python pruebas.py        (o:  python -m unittest pruebas -v)

Tecnica: pruebas ALEATORIAS contra un diccionario (oraculo). Despues de CADA operacion se valida
P2, P4, P5, el orden BST, los punteros padre y que el contenido sea identico al del dict.
"""
import math
import random
import unittest

from rn import ArbolRN, Color


class PruebasRN(unittest.TestCase):
    def test_pruebas_de_la_guia(self):
        a = ArbolRN()
        a.insertar(10, 10)
        self.assertIs(a.raiz.color, Color.NEGRO)                       # P1

        a2 = ArbolRN()
        for k in (10, 20, 30):
            a2.insertar(k, k)
        self.assertEqual(a2.raiz.clave, 20)                             # P2: raiz 20
        self.assertEqual(a2.altura_negra(), 1)                          # bh = 1

        a3 = ArbolRN()
        for k in (30, 10, 20):
            a3.insertar(k, k)
        self.assertEqual(a3.raiz.clave, 20)                             # P3: doble rotacion
        self.assertIs(a3.raiz.color, Color.NEGRO)

        a4 = ArbolRN()
        for k in range(1, 16):
            a4.insertar(k, k)
        self.assertGreaterEqual(a4.altura_negra(), 0)                   # P4
        self.assertIs(a4.raiz.color, Color.NEGRO)
        self.assertTrue(a4.es_valido())

    def test_secuencia_A_forma_exacta(self):
        a = ArbolRN()
        for k in (10, 20, 30, 15, 25):
            a.insertar(k, k)
        r = a.raiz
        self.assertEqual((r.clave, r.izq.clave, r.der.clave), (20, 10, 30))
        self.assertEqual((r.izq.der.clave, r.der.izq.clave), (15, 25))
        self.assertIs(r.izq.der.color, Color.ROJO)
        self.assertIs(r.der.izq.color, Color.ROJO)
        self.assertEqual(a.altura_negra(), 2)

    def test_secuencia_B_sin_rotaciones(self):
        b = ArbolRN()
        for k in (7, 3, 18, 10, 22, 8, 11, 26):
            b.insertar(k, k)
        self.assertEqual(b.rotaciones, 0)
        self.assertEqual(b.raiz.clave, 7)

    def test_duplicados_actualizan(self):
        a = ArbolRN()
        self.assertTrue(a.insertar(1, "uno"))
        self.assertFalse(a.insertar(1, "UNO"))
        self.assertEqual((len(a), a.buscar(1).valor), (1, "UNO"))

    def test_vacio(self):
        a = ArbolRN()
        self.assertEqual((len(a), a.altura(), a.minimo(), a.maximo()), (0, -1, None, None))
        self.assertFalse(a.eliminar(5))
        self.assertTrue(a.es_valido())

    def test_aleatorio_contra_dict(self):
        for semilla in range(1, 16):
            azar = random.Random(semilla)
            rango = 64 if semilla % 2 else 100000
            a, oraculo = ArbolRN(), {}
            for i in range(2500):
                k = azar.randint(0, rango)
                if azar.random() < 0.6:
                    self.assertEqual(a.insertar(k, i), k not in oraculo)
                    oraculo[k] = i
                else:
                    self.assertEqual(a.eliminar(k), oraculo.pop(k, None) is not None)
                self.assertIsNone(a.violacion(), f"semilla {semilla}, paso {i}")
                self.assertEqual(len(a), len(oraculo))
            self.assertEqual(list(a), sorted(oraculo.items()))

    def test_borrar_todo(self):
        for orden in (range(300), range(299, -1, -1)):
            a = ArbolRN()
            for k in range(300):
                a.insertar(k, k)
            for k in orden:
                self.assertTrue(a.eliminar(k))
                self.assertTrue(a.es_valido())
            self.assertEqual((len(a), a.altura()), (0, -1))

    def test_cota_de_altura(self):
        a = ArbolRN()
        for k in range(1, 5001):                    # peor caso para un BST: claves crecientes
            a.insertar(k, k)
        self.assertLessEqual(a.altura(), 2 * math.log2(len(a) + 1))
        self.assertTrue(a.es_valido())


if __name__ == "__main__":
    unittest.main(verbosity=2)
