// =====================================================================================
//  rn.h  -  Arbol Rojo-Negro generico en C++20  ("edicion avanzada")
//  SIS210 - Algoritmos y Estructuras de Datos | UNAP 2026-II | Semana 11
// =====================================================================================
//
//  QUE ES UN ARBOL ROJO-NEGRO?
//  Es un arbol binario de busqueda (BST) auto-balanceado. Cada nodo guarda un bit extra
//  (su COLOR) y, con unas reglas muy simples sobre esos colores, se garantiza que el
//  camino mas largo de la raiz a una hoja NUNCA supera el doble del camino mas corto.
//  Resultado: buscar, insertar y eliminar cuestan O(log n) en el PEOR caso.
//
//  LAS 5 PROPIEDADES (CLRS). Los NIL son los "huecos" (punteros nulos) bajo las hojas:
//    P1. Todo nodo es ROJO o NEGRO.                         (lo garantiza el enum Color)
//    P2. La raiz es NEGRA.
//    P3. Todo NIL (hoja vacia) se considera NEGRO.          (es_rojo(nullptr) == false)
//    P4. Un nodo ROJO solo puede tener hijos NEGROS         (no hay dos rojos seguidos).
//    P5. Todos los caminos de un nodo a sus NIL descendientes tienen el MISMO numero
//        de nodos negros. Ese numero es la ALTURA NEGRA bh(n).
//
//  POR QUE GARANTIZAN O(log n)?  Por P5 todos los caminos tienen bh nodos negros; por P4
//  los rojos nunca estan seguidos, asi que un camino puede ser a lo sumo el doble de largo
//  que otro (alternando negro-rojo). De ahi: altura h <= 2 * log2(n + 1).
//
//  ESTRUCTURA DE ESTE ARCHIVO (para exponerlo en orden):
//    0. Conceptos y utilidades .. ComparadorDe, BuscablePor, Imprimible, cota_altura, exigir
//    1. Tipos auxiliares ........ Color, Rotacion, CasoFixup, EventoFixup
//    2. NodoRN .................. el nodo (clave, valor, color, padre, hijos)
//    3. ArbolRN ................. la clase
//       3.1 Construccion, copia y movimiento (regla de los 5)
//       3.2 Iteradores y rangos . const_iterator bidireccional, claves(), entre(), ...
//       3.3 API publica ......... insertar / eliminar / buscar / alturas / recorridos
//       3.4 Utilidades privadas . es_rojo, ranura_de, minimo_desde, sucesor, predecesor
//       3.5 Rotaciones .......... TODO 1 y TODO 2 de la guia
//       3.6 insertar_fixup ...... TODO 3 de la guia (Casos 1, 2 y 3 + simetricos)
//       3.7 eliminar_fixup ...... ampliacion (Casos 1, 2, 3 y 4 + simetricos)
//       3.8 altura negra ........ TODO 4 de la guia
//       3.9 verificacion e impresion (para depurar y hacer diagramas)
//    4. Integracion con la biblioteca estandar (deduccion de plantillas, std::formatter)
//       y comprobaciones en tiempo de compilacion
//
// -------------------------------------------------------------------------------------
//  MAPA DE TECNICAS DE C++ MODERNO QUE USA ESTE ARCHIVO  (por si te las preguntan)
// -------------------------------------------------------------------------------------
//  [C++11/14/17]  plantillas con comparador, unique_ptr + RAII, enum class, constexpr,
//                 noexcept, [[nodiscard]], std::optional, enlace estructurado,
//                 std::initializer_list, referencias universales (F&&), if constexpr,
//                 expresiones fold, std::exchange, std::in_place, std::string_view.
//  [C++20]        * CONCEPTS: ComparadorDe<Comp,K>, BuscablePor<Comp,K,Q>, Imprimible<T>
//                   (restringen la plantilla y dan errores de compilacion legibles).
//                 * Clausulas `requires` finales en miembros (copiar solo si K y V se copian).
//                 * Operador nave espacial <=> y == por defecto (EventoFixup, comparar arboles).
//                 * RANGOS: el arbol ES un std::ranges::bidirectional_range; claves(),
//                   valores() y entre(a,b) devuelven vistas / subrangos componibles con `|`.
//                 * Iterador BIDIRECCIONAL con iterator_concept (sirve para --it y reverse).
//                 * BUSQUEDA HETEROGENEA (comparador transparente): buscar con string_view
//                   en un arbol<std::string,...> sin construir ningun std::string temporal.
//                 * try_emplace / emplace in situ con reenvio perfecto (std::forward).
//                 * std::span (eventos()), designated initializers (.campo = valor),
//                   `using enum`, std::source_location, [[likely]]/[[unlikely]],
//                   [[no_unique_address]], <bit> (std::bit_width) y constexpr.
//  [C++20 opc.]   * std::format + std::formatter<rn::Color> (solo si tu libstdc++ lo trae;
//                   se detecta solo con __cpp_lib_format; se puede apagar con -DRN_SIN_FORMAT).
//  Ver tambien rn_seguro.h (envoltorio con std::shared_mutex para usar el arbol con hilos).
//
//  DECISION DE DISENO IMPORTANTE: NO se usan asignadores propios (std::pmr, Allocator).
//  El modelo "cada nodo es dueno de sus hijos con unique_ptr" es lo que hace tan limpia la
//  explicacion de las rotaciones; un deleter con estado romperia esa claridad.
// =====================================================================================
#pragma once

// `_MSVC_LANG` existe en Visual Studio; los demas compiladores usan __cplusplus.
#if (defined(_MSVC_LANG) ? _MSVC_LANG : __cplusplus) < 202002L
#error "rn.h necesita C++20 o superior: compila con  -std=c++20  (g++ 11+ / clang 14+ / MSVC 19.30+)"
#endif

#include <algorithm>        // std::max, std::lexicographical_compare_three_way
#include <array>            // std::array (contadores por caso)
#include <bit>              // std::bit_width (cota de altura exacta con enteros)
#include <cassert>          // assert: precondiciones (se desactivan con -DNDEBUG)
#include <compare>          // std::strong_ordering, <=>
#include <concepts>         // std::same_as, std::convertible_to, std::predicate, ...
#include <cstddef>          // std::size_t, std::ptrdiff_t
#include <cstdint>          // std::uint64_t
#include <cstdio>           // std::fprintf (para exigir())
#include <cstdlib>          // std::abort
#include <functional>       // std::less
#include <initializer_list> // construir un arbol con { {k, v}, {k, v}, ... }
#include <iterator>         // std::bidirectional_iterator_tag, std::next, reverse_iterator
#include <memory>           // std::unique_ptr, std::make_unique
#include <optional>         // std::optional
#include <ostream>          // std::ostream
#include <ranges>           // std::ranges::*, std::views::*
#include <source_location>  // std::source_location (ubicacion del que llama)
#include <span>             // std::span (vista de solo lectura sobre un arreglo)
#include <stdexcept>        // std::out_of_range
#include <string>           // std::string
#include <string_view>      // std::string_view
#include <tuple>            // std::tuple_element_t (guia de deduccion)
#include <type_traits>      // std::is_nothrow_*_v, std::remove_const_t
#include <utility>          // std::move, std::swap, std::exchange, std::pair, std::in_place
#include <vector>           // std::vector
#include <version>          // macros de "feature test" (__cpp_lib_format, ...)

// std::format es opcional: GCC lo trae desde la version 13. Si no existe, el resto de la
// libreria funciona igual (solo se omite el formatter de la seccion 4).
#if defined(__cpp_lib_format) && !defined(RN_SIN_FORMAT)
#include <format>
#define RN_TIENE_FORMAT 1
#else
#define RN_TIENE_FORMAT 0
#endif

// Todo vive en el espacio de nombres `rn` para no chocar con otros nombres (Color, Nodo...)
namespace rn
{

    // -------------------------------------------------------------------------------------
    // 0. CONCEPTOS Y UTILIDADES                                                     [C++20]
    // -------------------------------------------------------------------------------------
    // Un CONCEPT es un predicado en tiempo de compilacion sobre tipos. Reemplaza al viejo
    // `static_assert(is_invocable...)` y a los trucos con SFINAE/enable_if: si alguien usa un
    // tipo que no cumple, el compilador dice EXACTAMENTE que requisito falla.

    // "Comp es un criterio de orden valido para claves de tipo K": se puede copiar y se puede
    // llamar como bool(const K&, const K&) sobre un objeto const (orden debil estricto).
    // std::strict_weak_order ya existe en <concepts>; aqui solo le agregamos "copiable".
    template <typename Comp, typename K>
    concept ComparadorDe =
        std::copy_constructible<Comp> && std::strict_weak_order<const Comp &, const K &, const K &>;

    // Un comparador es "transparente" si define el tipo anidado `is_transparent` (como std::less<>).
    // Eso es una PROMESA: "puedo comparar claves de tipos distintos entre si".
    template <typename C>
    concept EsTransparente = requires { typename C::is_transparent; };

    // "Se puede buscar en el arbol (clave K, comparador Comp) usando una clave de tipo Q":
    //   (a) Q es exactamente K, o
    //   (b) el comparador es transparente y sabe comparar Q con K en ambos sentidos
    //       (busqueda HETEROGENEA: no hace falta construir una K temporal), o
    //   (c) Q se convierte implicitamente a K (se convierte UNA vez antes de bajar por el arbol).
    template <typename Comp, typename K, typename Q>
    concept BuscablePor =
        std::same_as<Q, K> ||
        (EsTransparente<Comp> && std::predicate<const Comp &, const Q &, const K &> &&
         std::predicate<const Comp &, const K &, const Q &>) ||
        std::convertible_to<const Q &, K>;

    // En los casos (a) y (b) la busqueda NO convierte nada, por lo que no puede lanzar excepciones.
    template <typename Comp, typename K, typename Q>
    inline constexpr bool busqueda_directa_v = std::same_as<Q, K> || EsTransparente<Comp>;

    // "T se puede escribir con os << t" (hace falta para imprimir el arbol).
    template <typename T>
    concept Imprimible = requires(std::ostream &os, const T &t) {
        { os << t } -> std::convertible_to<std::ostream &>;
    };

    // Cota SUPERIOR EXACTA (con enteros) de la altura en aristas de un arbol rojo-negro con n nodos.
    //   Teorema:  h <= 2*log2(n+1)   <=>   2^h <= (n+1)^2   <=>   h <= floor(log2((n+1)^2))
    // y floor(log2(x)) = std::bit_width(x) - 1  (bit_width cuenta los bits que ocupa x).
    // Es constexpr: se puede evaluar en COMPILACION (ver los static_assert de abajo y de las pruebas).
    // Dominio: n < 2^32 - 1 para que (n+1)^2 quepa en 64 bits.
    [[nodiscard]] constexpr int cota_altura(std::uint64_t n) noexcept
    {
        assert(n < 0xFFFF'FFFFull); // separador de digitos ' desde C++14
        const std::uint64_t m = n + 1;
        return static_cast<int>(std::bit_width(m * m)) - 1;
    }
    static_assert(cota_altura(0) == 0 && cota_altura(15) == 8 && cota_altura(100) == 13,
                  "la cota de altura debe coincidir con 2*log2(n+1) redondeado hacia abajo");

    // `assert` con superpoderes: (1) NO se apaga con -DNDEBUG, (2) dice en que archivo y linea
    // esta la llamada, sin macros. El truco: un parametro por defecto `std::source_location::current()`
    // se evalua en el sitio de la LLAMADA, no dentro de la funcion.                          [C++20]
    inline void exigir(bool condicion, std::string_view mensaje,
                       std::source_location donde = std::source_location::current()) noexcept
    {
        if (condicion) [[likely]]
            return; // [[likely]]: pista al optimizador (el camino normal)
        std::fprintf(stderr, "[rn] %s:%u (%s)\n     %.*s\n", donde.file_name(),
                     static_cast<unsigned>(donde.line()), donde.function_name(),
                     static_cast<int>(mensaje.size()), mensaje.data());
        std::abort();
    }

    // Etiqueta para elegir el constructor a partir de un rango (imita a std::from_range de C++23).
    struct desde_rango_t
    {
        explicit desde_rango_t() = default;
    };
    inline constexpr desde_rango_t desde_rango{};

    // -------------------------------------------------------------------------------------
    // 1. TIPOS AUXILIARES
    // -------------------------------------------------------------------------------------

    // `enum class` (enumeracion fuertemente tipada): no se convierte a int por accidente y
    // sus valores no "ensucian" el espacio global: se escribe Color::ROJO, nunca solo ROJO.
    // `: unsigned char` hace que ocupe 1 byte (un nodo ya es bastante grande).
    enum class Color : unsigned char
    {
        ROJO,
        NEGRO
    };

    // Direccion de una rotacion (sirve para registrar lo que hizo el algoritmo).
    enum class Rotacion : unsigned char
    {
        NINGUNA,
        IZQUIERDA,
        DERECHA
    };

    // Cada "caso" de reparacion (fixup) que puede ejecutar el algoritmo.
    enum class CasoFixup : unsigned char
    {
        // --- Insercion (el nodo nuevo z es ROJO y puede violar P4) ---
        INS_CASO1_TIO_ROJO,  // padre y tio rojos      -> recolorear y subir
        INS_CASO2_TRIANGULO, // padre rojo, tio negro, z "interior" -> rotar al padre
        INS_CASO3_LINEA,     // padre rojo, tio negro, z "exterior" -> rotar al abuelo
        // --- Eliminacion (se perdio un nodo negro: x queda "doblemente negro") ---
        DEL_CASO1_HERMANO_ROJO,    // hermano rojo           -> rotar para volverlo negro
        DEL_CASO2_SOBRINOS_NEGROS, // hermano y sobrinos negros -> recolorear y subir
        DEL_CASO3_SOBRINO_CERCANO, // sobrino cercano rojo, lejano negro -> rotar al hermano
        DEL_CASO4_SOBRINO_LEJANO   // sobrino lejano rojo    -> rotar al padre y terminar
    };

    // Cantidad de casos. Se calcula a partir del ULTIMO enumerador, asi que si alguien agrega un
    // caso nuevo al final, el arreglo de contadores del arbol crece solo (y el static_assert avisa
    // si se olvida revisar este valor).
    inline constexpr std::size_t NUM_CASOS =
        static_cast<std::size_t>(CasoFixup::DEL_CASO4_SOBRINO_LEJANO) + 1;
    static_assert(NUM_CASOS == 7, "hay 3 casos de insercion + 4 de eliminacion");

    // Funciones `constexpr`: se pueden evaluar en compilacion; aqui solo dan nombres legibles.
    // Se usan ASCII puro (sin tildes) porque la consola de Windows puede deformar UTF-8.
    constexpr const char *nombre(CasoFixup c) noexcept
    {
        switch (c)
        {
            using enum CasoFixup; // [C++20] trae los enumeradores al ambito: se escribe INS_CASO1... a secas
        case INS_CASO1_TIO_ROJO:
            return "Caso 1 (tio rojo: recolorear)";
        case INS_CASO2_TRIANGULO:
            return "Caso 2 (triangulo)";
        case INS_CASO3_LINEA:
            return "Caso 3 (linea)";
        case DEL_CASO1_HERMANO_ROJO:
            return "Elim. Caso 1 (hermano rojo)";
        case DEL_CASO2_SOBRINOS_NEGROS:
            return "Elim. Caso 2 (sobrinos negros)";
        case DEL_CASO3_SOBRINO_CERCANO:
            return "Elim. Caso 3 (sobrino cercano rojo)";
        case DEL_CASO4_SOBRINO_LEJANO:
            return "Elim. Caso 4 (sobrino lejano rojo)";
        }
        return "?";
    }
    constexpr const char *nombre(Rotacion r) noexcept
    {
        switch (r)
        {
            using enum Rotacion;
        case NINGUNA:
            return "-";
        case IZQUIERDA:
            return "rotar_izquierda";
        case DERECHA:
            return "rotar_derecha";
        }
        return "?";
    }
    constexpr char letra(Color c) noexcept { return c == Color::ROJO ? 'R' : 'N'; }

    // Con estos operadores se puede escribir  std::cout << Color::ROJO  (la busqueda ADL los
    // encuentra porque viven en el mismo espacio de nombres que los tipos).
    inline std::ostream &operator<<(std::ostream &os, Color c) { return os << letra(c); }
    inline std::ostream &operator<<(std::ostream &os, CasoFixup c) { return os << nombre(c); }
    inline std::ostream &operator<<(std::ostream &os, Rotacion r) { return os << nombre(r); }

    // Un "evento" = un caso aplicado + la rotacion que lo acompano (si hubo).
    // `operator== = default` [C++20] genera la comparacion miembro a miembro; sirve en las pruebas
    // para escribir  assert(eventos == esperado).  Sigue siendo un agregado (se puede usar {a, b}).
    struct EventoFixup
    {
        CasoFixup caso;
        Rotacion rotacion;
        bool operator==(const EventoFixup &) const = default;
    };

    // -------------------------------------------------------------------------------------
    // 2. EL NODO
    // -------------------------------------------------------------------------------------
    // Modelo de propiedad (ownership):
    //   * Los HIJOS son `std::unique_ptr`: cada nodo es DUENO exclusivo de sus hijos, asi que
    //     al destruir la raiz se libera todo el arbol solo (RAII). Nunca hay `delete` manual.
    //   * El PADRE es un puntero crudo `NodoRN*`: solo OBSERVA, no es dueno. Hace falta para
    //     subir por el arbol en el fixup. Si el padre tambien fuera unique_ptr habria un ciclo.
    // Regla de oro: "puntero inteligente = duenio; puntero crudo = solo mira".
    template <typename K, typename V>
    struct NodoRN
    {
        K clave;
        V valor;
        Color color{Color::ROJO}; // un nodo nuevo SIEMPRE nace ROJO: asi no altera P5
        NodoRN *padre{nullptr};
        std::unique_ptr<NodoRN> izq;
        std::unique_ptr<NodoRN> der;

        // CONSTRUCCION IN SITU con reenvio perfecto: la clave y el valor se construyen DIRECTAMENTE
        // dentro del nodo, a partir de los argumentos, sin copias intermedias. `std::in_place_t` es
        // una "etiqueta" que evita que este constructor variadico se confunda con uno de copia.
        // `noexcept(...)` condicional: solo promete no lanzar si K y V no lanzan al construirse.
        template <typename KK, typename... Args>
        explicit NodoRN(std::in_place_t, KK &&k, Args &&...args) noexcept(
            std::is_nothrow_constructible_v<K, KK &&> && std::is_nothrow_constructible_v<V, Args &&...>)
            : clave(std::forward<KK>(k)),
              valor(std::forward<Args>(args)...) // sin args => valor() inicializa por valor (0, "", ...)
        {
        }
    };

    // -------------------------------------------------------------------------------------
    // 3. EL ARBOL
    // -------------------------------------------------------------------------------------
    // Parametros de plantilla:
    //   K    tipo de la clave           V    tipo del valor asociado
    //   Comp criterio de orden (por defecto std::less<K> => orden ascendente con operator<)
    //        `ComparadorDe<K> Comp` es sintaxis de concept: equivale a "typename Comp" mas el
    //        requisito ComparadorDe<Comp, K>.                                              [C++20]
    template <typename K, typename V, ComparadorDe<K> Comp = std::less<K>>
    class ArbolRN
    {
    public:
        using Nodo = NodoRN<K, V>;

    private:
        using PtrN = std::unique_ptr<Nodo>;

    public:
        // ---------------------------------------------------------------------------------
        // 3.1 CONSTRUCCION, COPIA Y MOVIMIENTO (regla de los 5)
        // ---------------------------------------------------------------------------------

        // `requires`: solo existe si Comp se puede construir sin argumentos (un lambda con captura,
        // por ejemplo, no se puede; en ese caso hay que usar el constructor ArbolRN(Comp)).
        ArbolRN()
            requires std::default_initializable<Comp>
        = default;
        explicit ArbolRN(Comp comp) : comp_(std::move(comp)) {}

        // Permite:  ArbolRN<int, std::string> a{ {3, "tres"}, {1, "uno"} };
        ArbolRN(std::initializer_list<std::pair<K, V>> iniciales, Comp comp = Comp())
            : comp_(std::move(comp))
        {
            for (const auto &[clave, valor] : iniciales)
                insertar(clave, valor); // enlace estructurado C++17
        }

        // Construir desde CUALQUIER rango de pares (vector, std::map, una vista...).  [C++20]
        //   ArbolRN<int,int> a(desde_rango, un_vector_de_pares);
        template <std::ranges::input_range R>
            requires std::convertible_to<std::ranges::range_reference_t<R>, std::pair<K, V>>
        ArbolRN(desde_rango_t, R &&rango, Comp comp = Comp()) : comp_(std::move(comp))
        {
            insertar_rango(std::forward<R>(rango));
        }

        // COPIA PROFUNDA. La version original la prohibia porque una copia superficial de punteros
        // seria un desastre; aqui se hace bien: se clona nodo por nodo, con la MISMA forma y los
        // MISMOS colores (O(n), sin volver a balancear). Solo existe si K y V se pueden copiar.
        ArbolRN(const ArbolRN &otro)
            requires std::copy_constructible<K> && std::copy_constructible<V>
            : raiz_(clonar(otro.raiz_.get(), nullptr)),
              tam_(otro.tam_),
              rotaciones_(otro.rotaciones_),
              conteo_casos_(otro.conteo_casos_),
              eventos_(otro.eventos_),
              registrar_(otro.registrar_),
              comp_(otro.comp_)
        {
        }

        // Asignacion por copia con el idioma "copiar y intercambiar": si copiar falla (por ejemplo,
        // falta de memoria) el arbol destino queda INTACTO (garantia fuerte de excepciones).
        ArbolRN &operator=(const ArbolRN &otro)
            requires std::copy_constructible<K> && std::copy_constructible<V> && std::swappable<Comp>
        {
            if (this != &otro)
            {
                ArbolRN copia(otro); // 1) puede lanzar: todavia no tocamos *this
                intercambiar(copia); // 2) no lanza: intercambiamos; `copia` se lleva lo viejo y lo libera
            }
            return *this;
        }

        // MOVER = robar la raiz del otro. Los nodos NO se mueven de lugar en memoria, por eso los
        // punteros `padre` siguen siendo validos. `std::exchange(x, nuevo)` devuelve el valor viejo
        // de x y le asigna `nuevo`: deja a `otro` como un arbol vacio pero valido.
        // El orden de la lista de inicializacion debe ser el orden de DECLARACION de los miembros.
        ArbolRN(ArbolRN &&otro) noexcept(std::is_nothrow_move_constructible_v<Comp>)
            : raiz_(std::move(otro.raiz_)),
              tam_(std::exchange(otro.tam_, 0)),
              rotaciones_(std::exchange(otro.rotaciones_, 0)),
              conteo_casos_(std::exchange(otro.conteo_casos_, {})),
              eventos_(std::exchange(otro.eventos_, {})),
              registrar_(otro.registrar_),
              comp_(std::move(otro.comp_)) {}

        ArbolRN &operator=(ArbolRN &&otro) noexcept(std::is_nothrow_move_constructible_v<Comp> &&
                                                    std::is_nothrow_swappable_v<Comp>)
        {
            ArbolRN temporal(std::move(otro)); // temporal se queda con lo de `otro`...
            intercambiar(temporal);            // ...y nosotros con lo de temporal; el viejo se destruye.
            return *this;
        }
        ~ArbolRN() = default; // destruye en cadena la raiz y todos los hijos (profundidad <= 2*log2 n)

        // ---------------------------------------------------------------------------------
        // 3.2 ITERADORES Y RANGOS                                                     [C++20]
        // ---------------------------------------------------------------------------------
        // Iterador inorden BIDIRECCIONAL (permite `for (const auto& nodo : arbol)`, `--it`, y
        // todos los algoritmos y vistas de <ranges>).
        // No guarda pila: usa los punteros `padre` para encontrar sucesor/predecesor. Ese es el truco.
        // Guarda ademas un puntero al arbol solo para que `--end()` pueda llegar al maximo.
        class const_iterator
        {
        public:
            using iterator_category = std::bidirectional_iterator_tag; // vocabulario "viejo" (C++17)
            using iterator_concept = std::bidirectional_iterator_tag;  // vocabulario de <ranges> (C++20)
            using value_type = Nodo;
            using difference_type = std::ptrdiff_t;
            using pointer = const Nodo *;
            using reference = const Nodo &;

            const_iterator() = default; // los iteradores deben poder crearse "vacios" (concepto std::regular)
            reference operator*() const noexcept { return *actual_; }
            pointer operator->() const noexcept { return actual_; }
            const_iterator &operator++() noexcept
            {
                actual_ = sucesor(actual_);
                return *this;
            }
            const_iterator operator++(int) noexcept
            {
                auto copia = *this;
                ++*this;
                return copia;
            }
            const_iterator &operator--() noexcept
            {
                // --end() es el MAXIMO del arbol; en cualquier otro caso es el predecesor.
                actual_ = (actual_ == nullptr) ? maximo_desde(arbol_->raiz_.get()) : predecesor(actual_);
                return *this;
            }
            const_iterator operator--(int) noexcept
            {
                auto copia = *this;
                --*this;
                return copia;
            }
            // Solo se escribe ==; el compilador deduce != solamente.                       [C++20]
            friend bool operator==(const const_iterator &a, const const_iterator &b) noexcept
            {
                return a.actual_ == b.actual_;
            }

        private:
            friend class ArbolRN; // solo el arbol puede construir iteradores con un nodo concreto
            const_iterator(const Nodo *n, const ArbolRN *a) noexcept : actual_(n), arbol_(a) {}
            const Nodo *actual_{nullptr};
            const ArbolRN *arbol_{nullptr};
        };
        using iterator = const_iterator; // las claves son inmutables: no hay iterador "de escritura"
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        [[nodiscard]] const_iterator begin() const noexcept { return const_iterator(minimo(), this); }
        [[nodiscard]] const_iterator end() const noexcept { return const_iterator(nullptr, this); }
        [[nodiscard]] const_iterator cbegin() const noexcept { return begin(); }
        [[nodiscard]] const_iterator cend() const noexcept { return end(); }
        [[nodiscard]] const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
        [[nodiscard]] const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }

        // `size()` hace que el arbol cumpla std::ranges::sized_range (los algoritmos saben el tamano en O(1)).
        [[nodiscard]] std::size_t size() const noexcept { return tam_; }

        // VISTAS (views): objetos livianos que "miran" el arbol sin copiarlo y se evaluan perezosamente.
        //   for (const auto& k : arbol.claves() | std::views::reverse) ...
        // OJO: la vista apunta al arbol; si el arbol se destruye o se modifica, la vista queda invalida.
        [[nodiscard]] auto claves() const
        {
            return std::views::transform(*this, [](const Nodo &n) -> const K &
                                         { return n.clave; });
        }
        [[nodiscard]] auto valores() const
        {
            return std::views::transform(*this, [](const Nodo &n) -> const V &
                                         { return n.valor; });
        }

        // Primer elemento con clave >= `clave` (o end()). Como std::map::lower_bound. O(log n).
        template <typename Q = K>
        [[nodiscard]] const_iterator lower_bound(const Q &clave) const
            requires BuscablePor<Comp, K, Q>
        {
            return con_clave(clave, [this](const auto &c)
                             {
            const Nodo* mejor = nullptr;
            for (const Nodo* n = raiz_.get(); n != nullptr;) {
                if (!comp_(n->clave, c)) { mejor = n; n = n->izq.get(); }  // n >= c: candidato; buscar uno menor
                else                     { n = n->der.get(); }
            }
            return const_iterator(mejor, this); });
        }

        // Primer elemento con clave > `clave` (o end()). O(log n).
        template <typename Q = K>
        [[nodiscard]] const_iterator upper_bound(const Q &clave) const
            requires BuscablePor<Comp, K, Q>
        {
            return con_clave(clave, [this](const auto &c)
                             {
            const Nodo* mejor = nullptr;
            for (const Nodo* n = raiz_.get(); n != nullptr;) {
                if (comp_(c, n->clave)) { mejor = n; n = n->izq.get(); }   // n > c: candidato; buscar uno menor
                else                    { n = n->der.get(); }
            }
            return const_iterator(mejor, this); });
        }

        // Todos los elementos con  desde <= clave <= hasta  como un SUBRANGO (se puede recorrer
        // con `for` y combinar con vistas). Cuesta O(log n) encontrar los extremos.
        //   for (const auto& n : arbol.entre(10, 20)) ...
        template <typename Q = K>
        [[nodiscard]] std::ranges::subrange<const_iterator> entre(const Q &desde, const Q &hasta) const
            requires BuscablePor<Comp, K, Q>
        {
            if (comp_(hasta, desde))
                return {end(), end()}; // intervalo invertido => vacio
            return {lower_bound(desde), upper_bound(hasta)};
        }

        // ---------------------------------------------------------------------------------
        // 3.3 API PUBLICA
        // ---------------------------------------------------------------------------------

        // Inserta (clave, valor). Si la clave YA existe, solo actualiza su valor.
        // Devuelve true si se creo un nodo nuevo y false si se actualizo uno existente.
        bool insertar(K clave, V valor)
        {
            // `colocar` solo CONSUME (mueve) clave y valor si de verdad crea un nodo; si la clave
            // ya estaba, `valor` sigue intacto y podemos asignarlo al nodo existente.
            auto [nodo, nuevo] = colocar(std::move(clave), std::move(valor));
            if (!nuevo)
                nodo->valor = std::move(valor);
            return nuevo;
        }

        // Inserta SOLO si la clave no existe y construye el valor in situ con `args...`.
        // Si la clave ya existe NO toca nada (a diferencia de insertar, que sobreescribe).
        // Devuelve (puntero al nodo, se_creo_un_nodo).                                   [C++17]
        template <typename... Args>
            requires std::constructible_from<V, Args &&...>
        std::pair<const Nodo *, bool> try_emplace(K clave, Args &&...args)
        {
            auto [nodo, nuevo] = colocar(std::move(clave), std::forward<Args>(args)...);
            return {nodo, nuevo};
        }

        // Inserta varios pares de una vez con una EXPRESION FOLD sobre el operador coma [C++17].
        //   a.insertar_varios(std::pair{1, "uno"}, std::pair{2, "dos"});
        // Devuelve cuantos nodos NUEVOS se crearon.
        template <typename... Pares>
            requires(std::convertible_to<Pares, std::pair<K, V>> && ...)
        std::size_t insertar_varios(Pares &&...pares)
        {
            std::size_t nuevos = 0;
            auto una = [&](std::pair<K, V> par)
            {
                nuevos += insertar(std::move(par.first), std::move(par.second)) ? 1 : 0;
            };
            (una(std::forward<Pares>(pares)), ...); // se expande a  una(p1), una(p2), una(p3)...
            return nuevos;
        }

        // Inserta todos los pares de un rango (vector, map, vista...).                  [C++20]
        template <std::ranges::input_range R>
            requires std::convertible_to<std::ranges::range_reference_t<R>, std::pair<K, V>>
        void insertar_rango(R &&rango)
        {
            for (auto &&elemento : rango)
            {
                auto par = static_cast<std::pair<K, V>>(std::forward<decltype(elemento)>(elemento));
                insertar(std::move(par.first), std::move(par.second));
            }
        }

        // Como std::map::operator[]: devuelve el valor de la clave, creandolo (V{}) si no existe.
        V &operator[](K clave)
            requires std::default_initializable<V>
        {
            return colocar(std::move(clave)).first->valor;
        }

        // Acceso con comprobacion: lanza std::out_of_range si la clave no existe.
        [[nodiscard]] V &at(const K &clave)
        {
            Nodo *n = buscar_nodo(clave);
            if (n == nullptr)
                throw std::out_of_range("ArbolRN::at: la clave no existe");
            return n->valor;
        }
        [[nodiscard]] const V &at(const K &clave) const
        {
            const Nodo *n = buscar_nodo(clave);
            if (n == nullptr)
                throw std::out_of_range("ArbolRN::at: la clave no existe");
            return n->valor;
        }

        // Elimina la clave. Devuelve true si existia. (Ampliacion: la guia solo pide insercion.)
        // `Q = K` por defecto: se puede llamar con K o (si el comparador es transparente) con otro tipo.
        template <typename Q = K>
        bool eliminar(const Q &clave)
            requires BuscablePor<Comp, K, Q>
        {
            Nodo *z = buscar_nodo(clave);
            if (z == nullptr)
                return false;
            eliminar_nodo(z);
            return true;
        }

        // Elimina el elemento al que apunta `pos` y devuelve el iterador al SIGUIENTE.
        // Funciona porque los nodos nunca se copian ni cambian de direccion: el sucesor sobrevive
        // aunque el arbol se reacomode (ver eliminar_nodo).
        const_iterator eliminar(const_iterator pos)
        {
            exigir(pos.actual_ != nullptr, "ArbolRN::eliminar(end()) no es valido");
            const_iterator siguiente = std::next(pos);
            // const_cast es legitimo: el nodo se creo como objeto NO const; solo lo vemos por un camino const.
            eliminar_nodo(const_cast<Nodo *>(pos.actual_));
            return siguiente;
        }

        // Elimina todos los elementos para los que `pred(clave, valor)` sea verdadero (como std::erase_if).
        // Devuelve cuantos se eliminaron.
        template <typename Pred>
            requires std::predicate<Pred &, const K &, const V &>
        std::size_t eliminar_si(Pred pred)
        {
            std::size_t borrados = 0;
            for (auto it = begin(); it != end();)
            {
                if (pred(it->clave, it->valor))
                {
                    it = eliminar(it);
                    ++borrados;
                }
                else
                {
                    ++it;
                }
            }
            return borrados;
        }

        // Busqueda BST estandar. Devuelve nullptr si no existe.
        // Devuelve `const Nodo*`: se puede LEER el nodo pero no alterar clave/color (romperia el arbol).
        // Con un comparador transparente (std::less<>) Q puede ser otro tipo (string_view, const char*...)
        // y NO se construye ninguna clave temporal.                                      [C++14/20]
        template <typename Q = K>
        [[nodiscard]] const Nodo *buscar(const Q &clave) const noexcept(busqueda_directa_v<Comp, K, Q>)
            requires BuscablePor<Comp, K, Q>
        {
            return buscar_nodo(clave);
        }
        template <typename Q = K>
        [[nodiscard]] bool contiene(const Q &clave) const noexcept(busqueda_directa_v<Comp, K, Q>)
            requires BuscablePor<Comp, K, Q>
        {
            return buscar_nodo(clave) != nullptr;
        }

        // Puntero al VALOR (modificable) de una clave, o nullptr. Se puede cambiar el valor sin
        // peligro; la clave y el color siguen siendo inaccesibles para escritura.
        template <typename Q = K>
        [[nodiscard]] V *valor_de(const Q &clave)
            requires BuscablePor<Comp, K, Q>
        {
            Nodo *n = buscar_nodo(clave);
            return n ? &n->valor : nullptr;
        }
        template <typename Q = K>
        [[nodiscard]] const V *valor_de(const Q &clave) const
            requires BuscablePor<Comp, K, Q>
        {
            const Nodo *n = buscar_nodo(clave);
            return n ? &n->valor : nullptr;
        }

        [[nodiscard]] const Nodo *raiz() const noexcept { return raiz_.get(); }
        [[nodiscard]] const Nodo *minimo() const noexcept { return raiz_ ? minimo_desde(raiz_.get()) : nullptr; }
        [[nodiscard]] const Nodo *maximo() const noexcept { return raiz_ ? maximo_desde(raiz_.get()) : nullptr; }
        [[nodiscard]] std::size_t tam() const noexcept { return tam_; }
        [[nodiscard]] bool vacio() const noexcept { return tam_ == 0; }
        void limpiar() noexcept
        {
            raiz_.reset();
            tam_ = 0;
        }
        [[nodiscard]] const Comp &comparador() const noexcept { return comp_; }

        // Altura del arbol en ARISTAS: arbol vacio = -1, un solo nodo = 0.
        [[nodiscard]] int altura() const noexcept { return altura_impl(raiz_.get()); }

        // bh(n): nodos negros desde n hasta una hoja NIL, SIN contar n y CONTANDO el NIL.
        // Devuelve -1 si el subarbol es inconsistente (P5 violada).
        [[nodiscard]] int altura_negra(const Nodo *n) const noexcept { return altura_negra_impl(n); }

        // El teorema h <= 2*log2(n+1), comprobado con la cota entera exacta (ver cota_altura).
        [[nodiscard]] bool altura_dentro_de_cota() const noexcept { return altura() <= cota_altura(tam_); }

        // ---- Recorridos. `visita` puede ser una lambda, una funcion o un functor (F&&: referencia universal) ----

        // Inorden: visita en orden ascendente de clave.  visita(clave, valor).
        template <typename F>
            requires std::invocable<F &, const K &, const V &>
        void inorden(F &&visita) const
        {
            inorden_impl(raiz_.get(), visita);
        }

        // Preorden: raiz, izquierda, derecha. Util para reconstruir exactamente la misma forma al reinsertar.
        template <typename F>
            requires std::invocable<F &, const K &, const V &>
        void preorden(F &&visita) const
        {
            preorden_impl(raiz_.get(), visita);
        }

        // Postorden: izquierda, derecha, raiz.
        template <typename F>
            requires std::invocable<F &, const K &, const V &>
        void postorden(F &&visita) const
        {
            postorden_impl(raiz_.get(), visita);
        }

        // Por niveles (BFS, de arriba hacia abajo y de izquierda a derecha).  visita(nodo, nivel).
        // Aqui el visitante recibe el NODO completo (con su color) y el nivel (la raiz es nivel 0).
        template <typename F>
            requires std::invocable<F &, const Nodo &, int>
        void por_niveles(F &&visita) const
        {
            if (!raiz_)
                return;
            // Cola implementada como vector + indice de lectura: evita std::queue y sus pop_front.
            std::vector<std::pair<const Nodo *, int>> cola{{raiz_.get(), 0}};
            for (std::size_t i = 0; i < cola.size(); ++i)
            {
                const auto [n, nivel] = cola[i]; // copia: emplace_back puede reubicar el vector
                visita(*n, nivel);
                if (n->izq)
                    cola.emplace_back(n->izq.get(), nivel + 1);
                if (n->der)
                    cola.emplace_back(n->der.get(), nivel + 1);
            }
        }

        // ---- Estadisticas del estado actual (recorre el arbol: O(n)) ----
        struct Estadisticas
        {
            std::size_t tam{};
            std::size_t rojos{};
            std::size_t negros{};
            int altura{-1};
            int altura_negra{0};
            std::size_t rotaciones{};
            bool operator==(const Estadisticas &) const = default;
        };
        [[nodiscard]] Estadisticas estadisticas() const
        {
            // Designated initializers [C++20]: se nombran los campos; los demas quedan con su valor por defecto.
            Estadisticas e{.tam = tam_, .altura = altura(), .altura_negra = std::max(0, altura_negra_impl(raiz_.get())), .rotaciones = rotaciones_};
            // El operador ?: devuelve una REFERENCIA al contador elegido, asi que se puede incrementar.
            for (const Nodo &n : *this)
                ++(n.color == Color::ROJO ? e.rojos : e.negros);
            return e;
        }

        // ---- Herramientas para depurar, probar y documentar ----

        // Devuelve std::nullopt si el arbol cumple BST + P2 + P4 + P5 + punteros padre + tamano;
        // si no, un texto que describe la PRIMERA violacion encontrada.
        [[nodiscard]] std::optional<std::string> violacion() const
        {
            if (!raiz_)
                return std::nullopt;
            if (raiz_->color != Color::NEGRO)
                return std::string("P2 violada: la raiz es roja");
            std::string mensaje;
            std::size_t contados = 0;
            if (verificar_impl(raiz_.get(), nullptr, nullptr, nullptr, mensaje, contados) < 0)
                return mensaje;
            if (contados != tam_)
                return std::string("el contador de tamano no coincide con los nodos reales");
            return std::nullopt;
        }
        [[nodiscard]] bool es_valido() const { return !violacion().has_value(); }

        // Aborta el programa (con archivo:linea de QUIEN LLAMA) si el arbol no es valido. No depende de NDEBUG.
        void exigir_valido(std::source_location donde = std::source_location::current()) const
        {
            if (const auto v = violacion())
                exigir(false, *v, donde);
        }

        // Dibuja el arbol en texto (necesita que K se pueda imprimir con operator<<).
        // Los nulos se muestran como "." solo cuando hace falta distinguir izquierda de derecha.
        void imprimir(std::ostream &os) const
            requires Imprimible<K>
        {
            if (!raiz_)
            {
                os << "(arbol vacio)\n";
                return;
            }
            imprimir_impl(os, raiz_.get(), "", Rama::RAIZ);
        }
        friend std::ostream &operator<<(std::ostream &os, const ArbolRN &a)
            requires Imprimible<K>
        {
            a.imprimir(os);
            return os;
        }

        // Escribe el arbol en formato Graphviz DOT (para generar diagramas para el informe).
        // Con incluir_nil = true tambien dibuja las hojas NIL (cuadritos negros).
        void exportar_dot(std::ostream &os, bool incluir_nil = false) const
            requires Imprimible<K>
        {
            os << "digraph ArbolRN {\n"
               << "  node [shape=circle, style=filled, fontcolor=white, fontname=\"Helvetica\", width=0.5];\n"
               << "  edge [arrowhead=none];\n";
            int contador = 0;
            if (raiz_)
                dot_impl(os, raiz_.get(), contador, incluir_nil);
            os << "}\n";
        }

        // ---- Registro de lo que hace el algoritmo (util para llenar la tabla de la guia) ----
        void registrar_eventos(bool activo) noexcept { registrar_ = activo; }
        // Casos aplicados por la ULTIMA insercion/eliminacion (requiere registrar_eventos(true)).
        // std::span = "puntero + longitud" de solo lectura: no copia el vector ni expone su tipo.   [C++20]
        [[nodiscard]] std::span<const EventoFixup> eventos() const noexcept { return eventos_; }
        // Cuantas veces se aplico cada caso desde que se creo el arbol. Este contador SIEMPRE funciona
        // (no hace falta registrar_eventos) y cuesta un solo incremento.
        [[nodiscard]] std::size_t conteo(CasoFixup caso) const noexcept
        {
            return conteo_casos_[static_cast<std::size_t>(caso)];
        }
        // Rotaciones acumuladas desde que se creo el arbol.
        [[nodiscard]] std::size_t rotaciones() const noexcept { return rotaciones_; }

        // ---- Comparacion de arboles por CONTENIDO (no por forma): dos arboles con las mismas
        //      claves y valores son iguales aunque se hayan insertado en distinto orden.      [C++20]
        friend bool operator==(const ArbolRN &a, const ArbolRN &b)
            requires std::equality_comparable<K> && std::equality_comparable<V>
        {
            return a.tam_ == b.tam_ && std::ranges::equal(a, b, [](const Nodo &x, const Nodo &y)
                                                          { return x.clave == y.clave && x.valor == y.valor; });
        }
        // Orden lexicografico (como comparar dos listas ordenadas) con el operador nave espacial <=>.
        friend auto operator<=>(const ArbolRN &a, const ArbolRN &b)
            requires std::three_way_comparable<K> && std::three_way_comparable<V>
        {
            using Categoria = std::common_comparison_category_t<std::compare_three_way_result_t<K>,
                                                                std::compare_three_way_result_t<V>>;
            return std::lexicographical_compare_three_way(
                a.begin(), a.end(), b.begin(), b.end(), [](const Nodo &x, const Nodo &y) -> Categoria
                {
                if (const auto c = x.clave <=> y.clave; c != 0) return c;  // `if` con inicializador (C++17)
                return x.valor <=> y.valor; });
        }

        friend void swap(ArbolRN &a, ArbolRN &b) noexcept(std::is_nothrow_swappable_v<Comp>) { a.intercambiar(b); }

    private:
        // ---------------------------------------------------------------------------------
        // 3.4 UTILIDADES PRIVADAS
        // ---------------------------------------------------------------------------------

        // P3 hecha codigo: un puntero nulo (NIL) cuenta como NEGRO, asi que "es rojo" es falso.
        static bool es_rojo(const Nodo *n) noexcept { return n != nullptr && n->color == Color::ROJO; }

        // Devuelve el unique_ptr que POSEE al nodo n: la raiz del arbol o el hijo izq/der de su padre.
        // Es la clave para rotar y reemplazar nodos sin perder la propiedad de la memoria.
        PtrN &ranura_de(Nodo *n) noexcept
        {
            Nodo *p = n->padre;
            if (p == nullptr)
                return raiz_;
            return (p->izq.get() == n) ? p->izq : p->der;
        }

        // Plantilla `N`: sirve tanto para Nodo* como para const Nodo* (evita duplicar la funcion).
        template <typename N>
        static N *minimo_desde(N *n) noexcept
        {
            while (n->izq)
                n = n->izq.get();
            return n;
        }
        template <typename N>
        static N *maximo_desde(N *n) noexcept
        {
            while (n->der)
                n = n->der.get();
            return n;
        }

        // Sucesor inorden: el nodo con la siguiente clave mayor, o nullptr si n es el maximo.
        static const Nodo *sucesor(const Nodo *n) noexcept
        {
            if (n->der)
                return minimo_desde(n->der.get()); // hay subarbol der: su minimo
            const Nodo *p = n->padre;              // si no, subir hasta venir "por la izquierda"
            while (p != nullptr && n == p->der.get())
            {
                n = p;
                p = p->padre;
            }
            return p;
        }
        // Predecesor inorden: simetrico del sucesor (se intercambian izq <-> der).
        static const Nodo *predecesor(const Nodo *n) noexcept
        {
            if (n->izq)
                return maximo_desde(n->izq.get());
            const Nodo *p = n->padre;
            while (p != nullptr && n == p->izq.get())
            {
                n = p;
                p = p->padre;
            }
            return p;
        }

        // Aplica `f` a la clave de busqueda. Si la busqueda NO es directa (Q != K y el comparador no es
        // transparente), convierte Q -> K UNA sola vez (puede crear un temporal) en lugar de convertir en
        // cada comparacion. `if constexpr` descarta en compilacion la rama que no se usa.   [C++17]
        template <typename Q, typename F>
        decltype(auto) con_clave(const Q &clave, F &&f) const
        {
            if constexpr (busqueda_directa_v<Comp, K, Q>)
            {
                return std::forward<F>(f)(clave);
            }
            else
            {
                const K &convertida = clave; // conversion implicita; el temporal vive hasta el final del bloque
                return std::forward<F>(f)(convertida);
            }
        }

        template <typename Q>
        Nodo *buscar_nodo(const Q &clave) const
        {
            // Lambda generica (`const auto&`): sirve para la clave K y para la clave Q.        [C++14]
            return con_clave(clave, [this](const auto &c) -> Nodo *
                             {
            Nodo* n = raiz_.get();
            while (n != nullptr) {
                if (comp_(c, n->clave))      n = n->izq.get();
                else if (comp_(n->clave, c)) n = n->der.get();
                else                         return n;
            }
            return nullptr; });
        }

        void intercambiar(ArbolRN &otro) noexcept(std::is_nothrow_swappable_v<Comp>)
        {
            using std::swap; // "using std::swap + llamada sin calificar": usa el swap propio del tipo si existe
            swap(raiz_, otro.raiz_);
            swap(tam_, otro.tam_);
            swap(rotaciones_, otro.rotaciones_);
            swap(conteo_casos_, otro.conteo_casos_);
            swap(eventos_, otro.eventos_);
            swap(registrar_, otro.registrar_);
            swap(comp_, otro.comp_);
        }

        void anotar(CasoFixup caso, Rotacion rot)
        {
            ++conteo_casos_[static_cast<std::size_t>(caso)];
            // emplace_back construye el EventoFixup DIRECTAMENTE dentro del vector (en C++20 un agregado
            // se puede construir con parentesis). Por defecto NO se registra: el costo es una comparacion.
            if (registrar_) [[unlikely]]
                eventos_.emplace_back(caso, rot);
        }

        // Atajo: rotar hacia la izquierda (true) o la derecha (false). Evita duplicar codigo
        // en los casos "espejo" (simetricos) de insercion y eliminacion.
        void rotar(Nodo *x, bool a_la_izquierda)
        {
            if (a_la_izquierda)
                rotar_izquierda(x);
            else
                rotar_derecha(x);
        }
        static constexpr Rotacion dir(bool a_la_izquierda) noexcept
        {
            return a_la_izquierda ? Rotacion::IZQUIERDA : Rotacion::DERECHA;
        }

        // Copia profunda recursiva (profundidad <= 2*log2 n gracias al balanceo).
        static PtrN clonar(const Nodo *origen, Nodo *padre_nuevo)
        {
            if (origen == nullptr)
                return nullptr;
            auto copia = std::make_unique<Nodo>(std::in_place, origen->clave, origen->valor);
            copia->color = origen->color;
            copia->padre = padre_nuevo;
            copia->izq = clonar(origen->izq.get(), copia.get());
            copia->der = clonar(origen->der.get(), copia.get());
            return copia; // `return` de una variable local: se MUEVE automaticamente, no se copia
        }

        // ---- Nucleo de la insercion (lo comparten insertar, try_emplace y operator[]) ----
        // Baja por el arbol; si la clave existe devuelve (nodo, false); si no, crea un nodo ROJO con
        // la clave y el valor construido con `args...`, lo enlaza, repara el arbol y devuelve (nodo, true).
        // GARANTIA FUERTE DE EXCEPCIONES: si construir el nodo lanza, el arbol queda como estaba (el nodo
        // se crea ANTES de tocar ningun enlace). El puntero devuelto sigue valido tras las rotaciones.
        template <typename... Args>
        std::pair<Nodo *, bool> colocar(K clave, Args &&...args)
        {
            if (registrar_) [[unlikely]]
                eventos_.clear();

            // (a) Descenso BST iterativo. `ranura` apunta al unique_ptr (hueco) donde caera el nodo.
            Nodo *padre = nullptr;
            PtrN *ranura = &raiz_;
            while (*ranura)
            {
                padre = ranura->get();
                if (comp_(clave, padre->clave))
                {
                    ranura = &padre->izq;
                }
                else if (comp_(padre->clave, clave))
                {
                    ranura = &padre->der;
                }
                else
                { // ni a < b ni b < a  =>  son iguales
                    return {padre, false};
                }
            }

            // (b) Crear el nodo (ROJO por defecto) y enlazarlo.
            *ranura = std::make_unique<Nodo>(std::in_place, std::move(clave), std::forward<Args>(args)...);
            Nodo *z = ranura->get();
            z->padre = padre;
            ++tam_;

            // (c) Reparar las propiedades que pudo romper el nodo rojo nuevo.
            insertar_fixup(z);
            return {z, true};
        }

        // ---- Nucleo de la eliminacion (lo comparten eliminar(clave) y eliminar(iterador)) ----
        // IMPORTANTE: ningun nodo SOBREVIVIENTE cambia de direccion; solo se reenlazan. Por eso los
        // iteradores a otros elementos siguen validos despues de eliminar.
        void eliminar_nodo(Nodo *z)
        {
            if (registrar_) [[unlikely]]
                eventos_.clear();

            Color color_perdido = z->color; // color del nodo que "desaparece" fisicamente del arbol
            Nodo *x = nullptr;              // nodo que ocupa el hueco (puede ser NIL = nullptr)
            Nodo *padre_x = nullptr;        // padre de x: lo guardamos aparte porque x puede ser nullptr

            if (!z->izq || !z->der)
            {
                // --- z tiene a lo sumo UN hijo: el hijo (o NIL) toma el lugar de z ---
                PtrN &ranura_z = ranura_de(z);
                PtrN hijo = z->izq ? std::move(z->izq) : std::move(z->der);
                padre_x = z->padre;
                x = hijo.get();
                if (hijo)
                    hijo->padre = padre_x;
                ranura_z = std::move(hijo); // al reasignar el unique_ptr, z se destruye aqui
            }
            else
            {
                // --- z tiene DOS hijos: su sucesor y (minimo del subarbol derecho) toma su lugar ---
                Nodo *y = minimo_desde(z->der.get());
                color_perdido = y->color; // el que sale de su sitio original es y
                x = y->der.get();         // y no tiene hijo izquierdo; su derecho subira
                PtrN y_dueno;             // aqui tomaremos la propiedad de y

                if (y->padre == z)
                {
                    y_dueno = std::move(z->der); // y es hijo directo de z; conserva su subarbol derecho
                    padre_x = y;
                }
                else
                {
                    padre_x = y->padre;
                    y_dueno = std::move(padre_x->izq);      // sacamos a y de su posicion
                    padre_x->izq = std::move(y_dueno->der); // el hijo derecho de y ocupa su lugar
                    if (padre_x->izq)
                        padre_x->izq->padre = padre_x;
                    y_dueno->der = std::move(z->der); // y adopta el subarbol derecho de z
                    y_dueno->der->padre = y_dueno.get();
                }
                y_dueno->izq = std::move(z->izq); // y adopta el subarbol izquierdo de z
                y_dueno->izq->padre = y_dueno.get();
                y_dueno->color = z->color; // y hereda el color de z (conserva P5 arriba)
                y_dueno->padre = z->padre;
                ranura_de(z) = std::move(y_dueno); // y ocupa el lugar de z; z se destruye
            }

            --tam_;
            // Solo si desaparecio un nodo NEGRO se rompe P5 y hay que reparar.
            if (color_perdido == Color::NEGRO)
                eliminar_fixup(x, padre_x);
        }

        // ---------------------------------------------------------------------------------
        // 3.5 ROTACIONES  (TODO 1 y TODO 2 de la guia)
        // ---------------------------------------------------------------------------------
        // Una rotacion cambia la FORMA del arbol sin alterar el orden inorden (sigue siendo BST).
        // Cuesta O(1): solo reasigna unos pocos punteros.
        /*
             rotar_izquierda(x):             x                 y
                                            / \               / \
                                           A   y     ==>     x   C
                                              / \           / \
                                             B   C         A   B

             rotar_derecha(y) es la operacion inversa (lee el dibujo de derecha a izquierda).
        */
        // Con unique_ptr no se pueden "copiar" punteros: hay que MOVER la propiedad paso a paso.

        // TODO 1 - x->izq sube, x baja a su derecha.  Precondicion: x->izq != nullptr
        void rotar_derecha(Nodo *x)
        {
            assert(x != nullptr && x->izq != nullptr);
            PtrN &ranura_x = ranura_de(x);          // quien posee a x (raiz_ o su padre)
            PtrN x_dueno = std::move(ranura_x);     // 1) sacamos a x del arbol (ranura_x queda nula)
            PtrN y_dueno = std::move(x_dueno->izq); // 2) separamos y = x->izq
            x_dueno->izq = std::move(y_dueno->der); // 3) el subarbol B de y pasa a ser hijo izq de x
            if (x_dueno->izq)
                x_dueno->izq->padre = x_dueno.get();
            y_dueno->padre = x_dueno->padre;   // 4) y hereda el padre que tenia x
            x_dueno->padre = y_dueno.get();    // 5) x ahora cuelga de y
            y_dueno->der = std::move(x_dueno); // 6) x pasa a ser hijo der de y
            ranura_x = std::move(y_dueno);     // 7) y ocupa el sitio donde estaba x
            ++rotaciones_;
        }

        // TODO 2 - x->der sube, x baja a su izquierda.  Precondicion: x->der != nullptr
        // (es la imagen en espejo de rotar_derecha: se cambian izq <-> der)
        void rotar_izquierda(Nodo *x)
        {
            assert(x != nullptr && x->der != nullptr);
            PtrN &ranura_x = ranura_de(x);
            PtrN x_dueno = std::move(ranura_x);
            PtrN y_dueno = std::move(x_dueno->der);
            x_dueno->der = std::move(y_dueno->izq);
            if (x_dueno->der)
                x_dueno->der->padre = x_dueno.get();
            y_dueno->padre = x_dueno->padre;
            x_dueno->padre = y_dueno.get();
            y_dueno->izq = std::move(x_dueno);
            ranura_x = std::move(y_dueno);
            ++rotaciones_;
        }

        // ---------------------------------------------------------------------------------
        // 3.6 REPARACION TRAS INSERTAR  (TODO 3 de la guia)
        // ---------------------------------------------------------------------------------
        // z nace ROJO: no toca P5 (no agrega nodos negros) pero puede violar
        //   P4 (si su padre es rojo)  o  P2 (si z es la raiz). Se sube por el arbol arreglando.
        // Notacion: p = padre de z, g = abuelo, tio = hermano de p.
        // Si p es ROJO entonces p NO es la raiz (la raiz es negra), asi que g siempre existe.
        //
        // Hay dos escenarios "espejo": p es hijo izquierdo de g, o hijo derecho. En lugar de
        // escribir todo dos veces, `padre_es_izq` decide hacia que lado rotar.
        void insertar_fixup(Nodo *z)
        {
            while (es_rojo(z->padre))
            {
                Nodo *p = z->padre;
                Nodo *g = p->padre;
                const bool padre_es_izq = (g->izq.get() == p);
                Nodo *tio = padre_es_izq ? g->der.get() : g->izq.get();

                if (es_rojo(tio))
                {
                    // CASO 1: tio ROJO. Solo recolorear: padre y tio -> negro, abuelo -> rojo.
                    // La altura negra no cambia, pero el abuelo (ahora rojo) puede tener un padre
                    // rojo: por eso se SUBE (z = g) y se repite el ciclo.
                    p->color = Color::NEGRO;
                    tio->color = Color::NEGRO;
                    g->color = Color::ROJO;
                    anotar(CasoFixup::INS_CASO1_TIO_ROJO, Rotacion::NINGUNA);
                    z = g;
                    continue;
                }

                // Desde aqui el tio es NEGRO (o NIL): hay que ROTAR.
                const bool z_es_interior = padre_es_izq ? (z == p->der.get())  // forma "<"
                                                        : (z == p->izq.get()); // forma ">"
                if (z_es_interior)
                {
                    // CASO 2 (triangulo): rotar en p para convertirlo en el CASO 3 (linea recta).
                    z = p;
                    rotar(z, /*a_la_izquierda=*/padre_es_izq);
                    anotar(CasoFixup::INS_CASO2_TRIANGULO, dir(padre_es_izq));
                    p = z->padre; // tras rotar, el viejo z es ahora el padre
                }
                // CASO 3 (linea): p negro, g rojo, y rotar en g hacia el lado contrario a p.
                p->color = Color::NEGRO;
                g->color = Color::ROJO;
                rotar(g, /*a_la_izquierda=*/!padre_es_izq);
                anotar(CasoFixup::INS_CASO3_LINEA, dir(!padre_es_izq));
                // Ahora el padre de z es negro: el ciclo termina solo.
            }
            raiz_->color = Color::NEGRO; // P2: el Caso 1 pudo pintar la raiz de rojo
        }

        // ---------------------------------------------------------------------------------
        // 3.7 REPARACION TRAS ELIMINAR  (ampliacion)
        // ---------------------------------------------------------------------------------
        // Si se quita un nodo NEGRO, el camino que pasaba por el pierde un negro (P5 rota).
        // Imaginamos que x (el nodo que ocupa el hueco) es "DOBLEMENTE NEGRO" y buscamos
        // quitar ese negro extra. x puede ser nullptr, por eso se pasa tambien su padre.
        // w = hermano de x (nunca es NIL aqui: por P5 su subarbol tiene al menos un negro).
        void eliminar_fixup(Nodo *x, Nodo *padre_x)
        {
            while (x != raiz_.get() && !es_rojo(x))
            {
                const bool x_es_izq = (padre_x->izq.get() == x);
                Nodo *w = x_es_izq ? padre_x->der.get() : padre_x->izq.get();

                if (es_rojo(w))
                {
                    // CASO 1: hermano ROJO -> pintar w negro y padre rojo, rotar en el padre.
                    // Asi el hermano nuevo es NEGRO y pasamos a los casos 2, 3 o 4.
                    w->color = Color::NEGRO;
                    padre_x->color = Color::ROJO;
                    rotar(padre_x, x_es_izq);
                    anotar(CasoFixup::DEL_CASO1_HERMANO_ROJO, dir(x_es_izq));
                    w = x_es_izq ? padre_x->der.get() : padre_x->izq.get();
                }

                // Sobrino "cercano" = el hijo de w que esta del mismo lado que x; "lejano" = el otro.
                Nodo *cercano = x_es_izq ? w->izq.get() : w->der.get();
                Nodo *lejano = x_es_izq ? w->der.get() : w->izq.get();

                if (!es_rojo(cercano) && !es_rojo(lejano))
                {
                    // CASO 2: hermano negro con dos sobrinos negros -> w rojo, el problema sube.
                    w->color = Color::ROJO;
                    anotar(CasoFixup::DEL_CASO2_SOBRINOS_NEGROS, Rotacion::NINGUNA);
                    x = padre_x;
                    padre_x = x->padre;
                }
                else
                {
                    if (!es_rojo(lejano))
                    {
                        // CASO 3: sobrino cercano rojo y lejano negro -> rotar en w para dejarlo como Caso 4.
                        cercano->color = Color::NEGRO;
                        w->color = Color::ROJO;
                        rotar(w, !x_es_izq);
                        anotar(CasoFixup::DEL_CASO3_SOBRINO_CERCANO, dir(!x_es_izq));
                        w = x_es_izq ? padre_x->der.get() : padre_x->izq.get();
                        lejano = x_es_izq ? w->der.get() : w->izq.get();
                    }
                    // CASO 4: sobrino lejano rojo -> w toma el color del padre, padre y lejano negros,
                    // rotar en el padre. El negro extra queda absorbido: se termina.
                    w->color = padre_x->color;
                    padre_x->color = Color::NEGRO;
                    lejano->color = Color::NEGRO;
                    rotar(padre_x, x_es_izq);
                    anotar(CasoFixup::DEL_CASO4_SOBRINO_LEJANO, dir(x_es_izq));
                    x = raiz_.get();
                    padre_x = nullptr;
                }
            }
            if (x != nullptr)
                x->color = Color::NEGRO; // x rojo (o raiz) simplemente se pinta de negro
        }

        // ---------------------------------------------------------------------------------
        // 3.8 ALTURA NEGRA  (TODO 4 de la guia) y altura normal
        // ---------------------------------------------------------------------------------
        // Convencion (CLRS): bh(n) NO cuenta a n pero SI cuenta el NIL final.
        //   bh(NIL) = 0.   Para un hijo c de n:  aporte = bh(c) + (c es negro ? 1 : 0)   (NIL es negro)
        // Si el aporte de los dos hijos difiere, P5 esta violada y se devuelve -1.
        // (La recursion es segura: la profundidad nunca pasa de 2*log2(n+1), ~40 niveles para 1e6 nodos.)
        int altura_negra_impl(const Nodo *n) const noexcept
        {
            if (n == nullptr)
                return 0;
            const int hi = altura_negra_impl(n->izq.get());
            const int hd = altura_negra_impl(n->der.get());
            if (hi < 0 || hd < 0)
                return -1; // inconsistencia ya detectada abajo
            const int por_izq = hi + (es_rojo(n->izq.get()) ? 0 : 1);
            const int por_der = hd + (es_rojo(n->der.get()) ? 0 : 1);
            return (por_izq == por_der) ? por_izq : -1;
        }

        static int altura_impl(const Nodo *n) noexcept
        {
            if (n == nullptr)
                return -1;
            return 1 + std::max(altura_impl(n->izq.get()), altura_impl(n->der.get()));
        }

        template <typename F>
        static void inorden_impl(const Nodo *n, F &visita)
        {
            if (n == nullptr)
                return;
            inorden_impl(n->izq.get(), visita);
            visita(n->clave, n->valor);
            inorden_impl(n->der.get(), visita);
        }
        template <typename F>
        static void preorden_impl(const Nodo *n, F &visita)
        {
            if (n == nullptr)
                return;
            visita(n->clave, n->valor);
            preorden_impl(n->izq.get(), visita);
            preorden_impl(n->der.get(), visita);
        }
        template <typename F>
        static void postorden_impl(const Nodo *n, F &visita)
        {
            if (n == nullptr)
                return;
            postorden_impl(n->izq.get(), visita);
            postorden_impl(n->der.get(), visita);
            visita(n->clave, n->valor);
        }

        // ---------------------------------------------------------------------------------
        // 3.9 VERIFICACION E IMPRESION
        // ---------------------------------------------------------------------------------
        // Revisa un subarbol y devuelve su bh, o -1 (dejando la causa en `mensaje`).
        // (inf, sup) son las cotas EXCLUSIVAS que impone el orden BST a este subarbol.
        int verificar_impl(const Nodo *n, const Nodo *padre_esperado, const K *inf, const K *sup,
                           std::string &mensaje, std::size_t &contados) const
        {
            if (n == nullptr)
                return 0;
            ++contados;
            if (n->padre != padre_esperado)
            {
                mensaje = "un puntero 'padre' es inconsistente";
                return -1;
            }
            if (inf != nullptr && !comp_(*inf, n->clave))
            {
                mensaje = "orden BST violado (clave demasiado chica)";
                return -1;
            }
            if (sup != nullptr && !comp_(n->clave, *sup))
            {
                mensaje = "orden BST violado (clave demasiado grande)";
                return -1;
            }
            if (n->color == Color::ROJO && (es_rojo(n->izq.get()) || es_rojo(n->der.get())))
            {
                mensaje = "P4 violada: un nodo rojo tiene un hijo rojo";
                return -1;
            }
            const int hi = verificar_impl(n->izq.get(), n, inf, &n->clave, mensaje, contados);
            if (hi < 0)
                return -1;
            const int hd = verificar_impl(n->der.get(), n, &n->clave, sup, mensaje, contados);
            if (hd < 0)
                return -1;
            const int por_izq = hi + (es_rojo(n->izq.get()) ? 0 : 1);
            const int por_der = hd + (es_rojo(n->der.get()) ? 0 : 1);
            if (por_izq != por_der)
            {
                mensaje = "P5 violada: caminos con distinta cantidad de nodos negros";
                return -1;
            }
            return por_izq;
        }

        enum class Rama
        {
            RAIZ,
            MEDIA,
            ULTIMA
        };

        static void imprimir_impl(std::ostream &os, const Nodo *n, const std::string &prefijo, Rama rama)
        {
            os << prefijo << (rama == Rama::RAIZ ? "" : rama == Rama::MEDIA ? "+-- "
                                                                            : "`-- ");
            if (n == nullptr)
            {
                os << ".\n";
                return;
            }
            os << n->clave << '(' << letra(n->color) << ")\n";
            if (!n->izq && !n->der)
                return;
            const std::string hijos = prefijo + (rama == Rama::RAIZ ? "" : rama == Rama::MEDIA ? "|   "
                                                                                               : "    ");
            imprimir_impl(os, n->izq.get(), hijos, Rama::MEDIA);
            imprimir_impl(os, n->der.get(), hijos, Rama::ULTIMA);
        }

        void dot_impl(std::ostream &os, const Nodo *n, int &contador, bool incluir_nil) const
        {
            const int id = contador++;
            os << "  n" << id << " [label=\"" << n->clave << "\", fillcolor=\""
               << (n->color == Color::ROJO ? "red" : "black") << "\"];\n";
            for (const Nodo *hijo : {n->izq.get(), n->der.get()})
            {
                if (hijo != nullptr)
                {
                    const int id_hijo = contador; // el hijo tomara este id al entrar
                    os << "  n" << id << " -> n" << id_hijo << ";\n";
                    dot_impl(os, hijo, contador, incluir_nil);
                }
                else if (incluir_nil)
                {
                    const int id_nil = contador++;
                    os << "  n" << id_nil << " [label=\"NIL\", shape=box, fillcolor=\"black\", fontsize=8, width=0.25, height=0.2];\n"
                       << "  n" << id << " -> n" << id_nil << ";\n";
                }
            }
        }

        // ---------------------------------------------------------------------------------
        // DATOS MIEMBRO (el orden de declaracion es el orden de inicializacion)
        // ---------------------------------------------------------------------------------
        PtrN raiz_;                                         // dueno de todo el arbol
        std::size_t tam_{0};                                // cantidad de nodos
        std::size_t rotaciones_{0};                         // estadistica: rotaciones realizadas
        std::array<std::size_t, NUM_CASOS> conteo_casos_{}; // cuantas veces se aplico cada caso
        std::vector<EventoFixup> eventos_;                  // casos de la ultima operacion
        bool registrar_{false};                             // por defecto NO se registra (cero costo extra)
        // [[no_unique_address]] [C++20]: si Comp es una clase VACIA (como std::less<int>), no ocupa
        // espacio en el objeto. Sin esto, un comparador sin datos costaria 8 bytes por relleno.
        [[no_unique_address]] Comp comp_{};
    };

    // -------------------------------------------------------------------------------------
    // 4. INTEGRACION CON LA BIBLIOTECA ESTANDAR
    // -------------------------------------------------------------------------------------

    // GUIAS DE DEDUCCION (CTAD): dejan escribir el arbol SIN indicar <K, V>.            [C++17]
    //   ArbolRN a{ std::pair{1, std::string("uno")}, std::pair{2, std::string("dos")} };   // ArbolRN<int, string>
    template <typename K, typename V>
    ArbolRN(std::initializer_list<std::pair<K, V>>) -> ArbolRN<K, V>;

    // Desde un rango de pares (std::map<int, double> -> ArbolRN<int, double>). Los pares de un
    // std::map tienen la clave `const`; remove_const_t la quita para obtener el tipo de clave.
    template <std::ranges::input_range R>
    ArbolRN(desde_rango_t, R &&) -> ArbolRN<std::remove_const_t<std::tuple_element_t<0, std::ranges::range_value_t<R>>>,
                                            std::tuple_element_t<1, std::ranges::range_value_t<R>>>;

    // COMPROBACIONES EN TIEMPO DE COMPILACION. Si alguien rompe el iterador o la clase, el error
    // aparece al incluir el archivo, con un mensaje, y no 300 lineas despues dentro de un algoritmo.
    namespace detalle
    {
        using ArbolDePrueba = ArbolRN<int, int>;
        static_assert(std::bidirectional_iterator<ArbolDePrueba::const_iterator>,
                      "const_iterator debe cumplir std::bidirectional_iterator");
        static_assert(std::ranges::bidirectional_range<ArbolDePrueba>,
                      "ArbolRN debe ser un std::ranges::bidirectional_range (usable con vistas y algoritmos)");
        static_assert(std::ranges::sized_range<ArbolDePrueba>, "ArbolRN debe conocer su tamano en O(1)");
        static_assert(std::ranges::common_range<ArbolDePrueba>, "begin() y end() deben tener el mismo tipo");
        static_assert(std::is_nothrow_move_constructible_v<ArbolDePrueba>, "mover un arbol no debe lanzar");
        static_assert(ComparadorDe<std::less<int>, int> && !ComparadorDe<int, int>,
                      "std::less<int> es un comparador valido; un simple int no");
    } // namespace detalle

} // namespace rn

// std::formatter: hace que  std::format("{}", rn::Color::ROJO)  funcione, y con ancho/alineacion
// ("{:>8}") gratis, porque heredamos del formatter de string_view.                       [C++20]
// Las especializaciones de plantillas de std se escriben FUERA de nuestro espacio de nombres.
#if RN_TIENE_FORMAT
template <>
struct std::formatter<rn::Color> : std::formatter<std::string_view>
{
    auto format(rn::Color c, std::format_context &ctx) const
    {
        return std::formatter<std::string_view>::format(c == rn::Color::ROJO ? "ROJO" : "NEGRO", ctx);
    }
};
template <>
struct std::formatter<rn::Rotacion> : std::formatter<std::string_view>
{
    auto format(rn::Rotacion r, std::format_context &ctx) const
    {
        return std::formatter<std::string_view>::format(rn::nombre(r), ctx);
    }
};
template <>
struct std::formatter<rn::CasoFixup> : std::formatter<std::string_view>
{
    auto format(rn::CasoFixup c, std::format_context &ctx) const
    {
        return std::formatter<std::string_view>::format(rn::nombre(c), ctx);
    }
};
#endif
