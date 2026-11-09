# SIS210 - Semana 11 - Árboles Rojo-Negro (C++20 y Python 3.12)

Código completo para la práctica y la exposición. Todo fue compilado y ejecutado antes de empaquetarlo:
C++ con `-Wall -Wextra -Wpedantic` sin avisos en **g++ 13 y clang++ 18**, con AddressSanitizer + UBSan sin errores,
con ThreadSanitizer sin carreras de datos, y en `-O0`/`-O2`/`-O3` y `-std=c++20`/`-std=c++23`. Python con 8 pruebas automáticas.

> **Edición avanzada de C++:** la parte C++ pasó de C++17 a **C++20** (concepts, rangos, `<=>`, búsqueda heterogénea,
> copia profunda, iterador bidireccional, envoltorio con hilos…). El algoritmo rojo-negro en sí (rotaciones, `insertar_fixup`,
> `eliminar_fixup`) **no cambió**: produce exactamente los mismos árboles que antes. La parte Python no se tocó.

```
cpp/
  rn.h                    <- LA LIBRERÍA (ArbolRN<K,V,Comp>), muy comentada. Es lo que vas a exponer.
  rn_seguro.h             <- ArbolRNSeguro: el mismo árbol protegido con std::shared_mutex (para hilos)
  rn.cpp                  <- pruebas de la guía (P1..P4), Secuencia A y B con trazas, eliminación, iterador,
                             + sección nueva con las capacidades C++20
  pruebas_avanzadas.cpp   <- static_assert + pruebas aleatorias contra std::map + tabla de alturas (n hasta 1 000 000)
  pruebas_concurrencia.cpp<- 4 escritores y 4 lectores a la vez sobre ArbolRNSeguro (std::jthread, std::latch)
  siga_rn.cpp             <- Actividad 4 en C++ (datos validados en compilación, string_view, vistas, <chrono>)
python/
  rn.py                   <- el mismo árbol en Python (iterativo)
  bst.py, avl.py          <- árboles de comparación para la Actividad 3
  experimento.py          <- Actividad 3 (BST vs AVL vs RN) con tabla final
  siga_rn.py              <- Actividad 4
  demo_secuencias.py      <- Actividad 1 (Secuencia A y B paso a paso)
  pruebas.py              <- pruebas automáticas (unittest)
```

## Cómo correrlo (terminal de VS Code, desde la carpeta de cada parte)

**C++** (necesita `g++` **11 o superior** o `clang++` 14+, con `-std=c++20`; en Windows, MSYS2/MinGW-w64 reciente o WSL):
```
cd cpp
g++ -std=c++20 -Wall -Wextra -Wpedantic -O2 rn.cpp -o rn                         && ./rn
g++ -std=c++20 -Wall -Wextra -Wpedantic -O2 pruebas_avanzadas.cpp -o pruebas     && ./pruebas
g++ -std=c++20 -Wall -Wextra -Wpedantic -O2 siga_rn.cpp -o siga_rn               && ./siga_rn
g++ -std=c++20 -Wall -Wextra -Wpedantic -O2 -pthread pruebas_concurrencia.cpp -o concurrencia && ./concurrencia
```
En PowerShell usa `.\rn.exe`, `.\pruebas.exe`, `.\siga_rn.exe`, `.\concurrencia.exe`. No compiles con `-DNDEBUG` (apaga los `assert`).

Notas de compatibilidad:
- `std::format` (solo se usa en una demostración opcional de `rn.cpp` y en los `std::formatter` de `rn.h`) existe desde **g++ 13**. Con un compilador
  más viejo se detecta solo (`__cpp_lib_format`) y esa parte se omite; también se puede apagar con `-DRN_SIN_FORMAT`.
- `pruebas_concurrencia.cpp` necesita hilos de C++ (`-pthread` en Linux/macOS). En Windows usa un MinGW con modelo de hilos **posix**
  (MSYS2 lo trae); si tu `g++ --version` dice "win32" no compilará esa prueba, pero las otras tres sí.
- Si ves `error: ... requires C++20`, falta el `-std=c++20`.

**Python**:
```
cd python
python pruebas.py
python demo_secuencias.py
python experimento.py
python siga_rn.py
```

Cada programa también escribe archivos `.dot` (Graphviz) con los diagramas de los árboles, listos para el informe:
`dot -Tpng arbol_secuencia_A.dot -o arbol_A.png` (si no tienes Graphviz, pega el contenido en un visor en línea de Graphviz).

## Qué debería salir (para que verifiques tus resultados)

| Prueba | Resultado esperado |
|---|---|
| `./rn` | termina con `Todas las pruebas de rn.cpp pasaron.` |
| Secuencia A (10, 20, 30, 15, 25) | raíz 20(N); 10(N) con hijo der 15(R); 30(N) con hijo izq 25(R); altura 2, bh 2 |
| Secuencia B (7, 3, 18, 10, 22, 8, 11, 26) | solo **Caso 1** (al insertar 10 y 8); 0 rotaciones; raíz 7(N) siempre |
| `1..15` crecientes | altura 5, bh 3 |
| `./pruebas` | `Verificaciones ejecutadas: 127 | fallos: 0` y `RESULTADO: TODO OK` |
| Alturas RN, claves crecientes (aristas) | n=100 → 10; n=1000 → 16; n=10000 → 23; n=1 000 000 → 36 |
| SIGA (10 estudiantes) | raíz PU-20210312(N), altura 3, bh 2; al final 1 baja (promedio < 12) y quedan 9 |
| `./concurrencia` | `arboles invalidos vistos: 0`, `lecturas incoherentes: 0`, tamaño final 40000 y `RESULTADO: TODO OK` |

Los tiempos en nanosegundos del SIGA y el número de consultas de `./concurrencia` dependen de tu computadora; el resto es determinista.

## Orden recomendado para exponer `rn.h`

1. Las 5 propiedades y por qué dan O(log n) (comentario inicial) y el **mapa de técnicas de C++** de la cabecera.
2. Sección 0: los *concepts* (`ComparadorDe`, `BuscablePor`, `Imprimible`) y la cota `cota_altura` calculada en compilación.
3. `Color`, `NodoRN` y el modelo de propiedad: hijos `unique_ptr` (dueños), `padre` puntero crudo (observador).
4. `insertar` → `colocar`: descenso BST → crear nodo rojo → `insertar_fixup`.
5. `rotar_izquierda` / `rotar_derecha` (mover propiedad paso a paso con `std::move`).
6. `insertar_fixup`: Caso 1 (recolorear), Caso 2 (triángulo), Caso 3 (línea) y cómo `padre_es_izq` evita escribir los casos espejo dos veces.
7. `altura_negra_impl` (el NIL cuenta como negro) y `violacion()` (verifica P2, P4, P5, BST, padres).
8. Ampliaciones: `eliminar_nodo` + `eliminar_fixup` (4 casos), iterador bidireccional con sucesor/predecesor, vistas y `entre()`,
   copia profunda y movimiento, `exportar_dot`.
9. Opcional: `rn_seguro.h` (cerrojo lector/escritor).

## Técnicas de C++ que aparecen (por si te las preguntan)

**C++11/14/17:** plantillas con comparador, `unique_ptr` y RAII, `enum class`, `constexpr`, `[[nodiscard]]`, `noexcept`, `std::optional`,
enlace estructurado, `std::initializer_list`, referencias universales (`F&&`) y reenvío perfecto, `if constexpr`, expresiones *fold*,
`std::exchange`, `std::in_place`, `std::string_view`, guías de deducción (CTAD), copiar-e-intercambiar.

**C++20 (nuevo en esta edición):**

| Técnica | Dónde | Para qué sirve |
|---|---|---|
| *Concepts* y cláusulas `requires` | `ComparadorDe`, `BuscablePor`, `Imprimible`; copia solo si K y V se copian | errores de compilación legibles y API que se adapta a los tipos |
| Búsqueda heterogénea (`std::less<>`, `is_transparent`) | `buscar`, `eliminar`, `lower_bound`… | buscar con `string_view` sin crear un `std::string` (probado contando construcciones) |
| Rangos y vistas | `claves()`, `valores()`, `entre(a,b)`, `begin/end/rbegin/rend` | `arbol.claves() \| views::filter(...) \| views::reverse`, algoritmos `std::ranges::*` |
| Iterador bidireccional (`iterator_concept`) | `const_iterator` con `operator--` | `--end()`, recorrido inverso, `std::reverse_iterator` |
| `<=>` y `== = default` | comparar árboles por contenido; `EventoFixup`; `Estadisticas` | dos árboles con las mismas claves son iguales aunque su forma difiera |
| `try_emplace`, `operator[]`, `at`, `insertar_varios`, `insertar_rango` | API pública | construir el valor en el nodo sin copias; claves y valores *move-only* |
| `eliminar(iterador)` y `eliminar_si` | API pública | borrar mientras se recorre (el sucesor sobrevive porque los nodos no se copian) |
| `std::span`, *designated initializers*, `using enum` | `eventos()`, `estadisticas()`, `nombre()` | vistas sin copia, código más claro |
| `[[no_unique_address]]`, `[[likely]]/[[unlikely]]` | `comp_`, `exigir`, `anotar` | ahorrar memoria con comparadores vacíos y guiar al optimizador |
| `std::source_location` | `exigir`, `exigir_valido` | un `assert` que dice archivo:línea del que llama y no depende de `NDEBUG` |
| `<bit>` + `constexpr` + `static_assert` | `cota_altura`; datos del SIGA | calcular y validar en compilación |
| `std::jthread`, `std::stop_token`, `std::latch`, `std::shared_mutex` | `rn_seguro.h`, `pruebas_concurrencia.cpp` | árbol usable desde varios hilos |
| `std::format` + `std::formatter` (opcional) | `rn.h` sección 4 | `std::format("{:>6}", Color::ROJO)` |

**Decisión de diseño (por si te preguntan "¿y los asignadores / `std::pmr`?"):** no se incluyeron a propósito. Un *deleter* con estado haría cada
`unique_ptr` el doble de grande y rompería la claridad de las rotaciones con `std::move`; además `std::pmr::polymorphic_allocator` no es
asignable, lo que impide guardarlo dentro del `unique_ptr`. Habría que abandonar el modelo de propiedad que hace tan limpio este árbol.

**Lección de concurrencia:** el `std::shared_mutex` de glibc da preferencia a los lectores. Si 4 lectores consultan sin pausa, un escritor puede
**no entrar nunca** (inanición) y el programa parece colgado. Por eso los lectores de `pruebas_concurrencia.cpp` hacen una pausa corta; está explicado en el código.

## Notas sobre la guía (errores que encontré al simularla)

1. **Secuencia A:** el "criterio de corrección" de la guía describe un árbol incorrecto. El real es el de la tabla de arriba.
2. **Secuencia B:** la guía dice que provoca "los tres casos"; en realidad solo ocurre el Caso 1 dos veces y no hay rotaciones.
3. **Prueba P2** (insertar 10, 20, 30): se llama "Caso 1 (tío rojo)", pero el tío es NIL (negro): es una rotación (Caso 3). Los `assert` sí se cumplen.
4. **Cotas AVL** de la tabla de la Actividad 3 (23, 30, 37): con la fórmula del propio script salen 9.6, 14.4 y 19.1. Las de RN (13, 20, 27) son las cotas
   redondeadas hacia arriba; la cota **entera exacta** (`cota_altura`) es 13, 19 y 26, y la altura real nunca la supera.
5. La altura se mide aquí en **aristas** (un nodo = 0, vacío = -1). Si tu docente usa niveles, súmale 1; las cotas se siguen cumpliendo.
6. `insertar` y `buscar` no tienen TODO en la guía pero también hay que implementarlos (están en `rn.h`).
7. Pequeña diferencia de formato respecto a la edición anterior: la tabla de la Secuencia B en `./rn` ahora tiene las columnas bien alineadas (antes la cabecera estaba corrida un carácter).
