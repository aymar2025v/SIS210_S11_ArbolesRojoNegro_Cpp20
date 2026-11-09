// =====================================================================================
//  pruebas_avanzadas.cpp - Pruebas de estres y de propiedades del arbol Rojo-Negro (C++20)
//  Compilar:  g++ -std=c++20 -Wall -Wextra -Wpedantic -O2 pruebas_avanzadas.cpp -o pruebas
//  Ejecutar:  ./pruebas
//
//  Cuatro tecnicas de prueba, de la mas "barata" a la mas "pesada":
//    1. PRUEBAS EN TIEMPO DE COMPILACION (static_assert): si algo esta mal, ni compila.
//    2. PRUEBAS DE SEMANTICA: casos concretos (vacio, duplicados, mover, copiar, excepciones...).
//    3. PRUEBAS DE PROPIEDADES ("property-based testing"): en vez de 4 casos a mano, se hacen
//       miles de operaciones ALEATORIAS y despues de CADA una se comprueba que
//         (a) el arbol cumple P2, P4, P5, orden BST y punteros padre  (violacion())
//         (b) su contenido coincide con el de un std::map (el "oraculo" que sabemos correcto)
//    4. EXPERIMENTO: tabla de alturas hasta n = 1 000 000 comparada con el teorema h <= 2 log2(n+1).
// =====================================================================================
#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "rn.h"

using namespace rn;
namespace vistas = std::views;

// -------------------------------------------------------------------------------------
// 1. PRUEBAS EN TIEMPO DE COMPILACION
// -------------------------------------------------------------------------------------
namespace comprobaciones_estaticas
{

    using A = ArbolRN<int, int>;

    // El iterador y el arbol cumplen los conceptos de la biblioteca de rangos.
    static_assert(std::bidirectional_iterator<A::const_iterator>);
    static_assert(std::ranges::bidirectional_range<A> && std::ranges::bidirectional_range<const A>);
    static_assert(std::ranges::sized_range<A> && std::ranges::common_range<A>);
    static_assert(std::bidirectional_iterator<A::const_reverse_iterator>);

    // Copia y movimiento: la copia solo existe si K y V se pueden copiar (clausula `requires`).
    static_assert(std::is_copy_constructible_v<A> && std::is_copy_assignable_v<A>);
    static_assert(std::is_nothrow_move_constructible_v<A> && std::is_nothrow_move_assignable_v<A>);
    using SoloMovible = ArbolRN<int, std::unique_ptr<int>>;
    static_assert(!std::is_copy_constructible_v<SoloMovible> && !std::is_copy_assignable_v<SoloMovible>);
    static_assert(std::is_move_constructible_v<SoloMovible> && std::is_move_assignable_v<SoloMovible>);

    // El constructor por defecto solo existe si el comparador se puede crear sin argumentos.
    struct CmpConEstado
    {
        explicit CmpConEstado(int) {}
        bool operator()(int a, int b) const { return a < b; }
    };
    static_assert(!std::is_default_constructible_v<ArbolRN<int, int, CmpConEstado>>);
    static_assert(std::is_constructible_v<ArbolRN<int, int, CmpConEstado>, CmpConEstado>);

    // Conceptos propios.
    static_assert(ComparadorDe<std::less<int>, int> && ComparadorDe<std::greater<>, int>);
    static_assert(!ComparadorDe<int, int>, "un entero no es un comparador");
    static_assert(BuscablePor<std::less<>, std::string, std::string_view>, "comparador transparente => Q libre");
    static_assert(!BuscablePor<std::less<std::string>, std::string, std::string_view>,
                  "string_view -> string es explicito y el comparador no es transparente");
    static_assert(BuscablePor<std::less<std::string>, std::string, const char *>, "const char* -> string es implicito");
    struct SinFlujo
    {
    };
    static_assert(Imprimible<int> && Imprimible<std::string> && !Imprimible<SinFlujo>);

    // Funciones constexpr evaluadas por el compilador: la cota del teorema h <= 2 log2(n+1).
    static_assert(cota_altura(15) == 8 && cota_altura(1'000'000) == 39 && cota_altura(0) == 0);
    static_assert(nombre(CasoFixup::INS_CASO3_LINEA)[0] == 'C' && letra(Color::NEGRO) == 'N');
    static_assert(NUM_CASOS == 7);

} // namespace comprobaciones_estaticas

// Contador de fallos propio: a diferencia de assert, NO se desactiva con -DNDEBUG
// y permite seguir ejecutando para ver todos los fallos juntos.
// Macro VARIADICA (...): asi la condicion puede contener comas (por ejemplo, plantillas con varios argumentos).
static int g_fallos = 0;
static int g_pruebas = 0;
#define VERIFICAR(...)                                                           \
    do                                                                           \
    {                                                                            \
        ++g_pruebas;                                                             \
        if (!(__VA_ARGS__))                                                      \
        {                                                                        \
            ++g_fallos;                                                          \
            std::cerr << "FALLO (linea " << __LINE__ << "): " #__VA_ARGS__ "\n"; \
        }                                                                        \
    } while (false)

// Compara el contenido del arbol con el del std::map (en orden normal Y en orden inverso)
// y valida las propiedades RN.
static bool coincide(const ArbolRN<int, int> &arbol, const std::map<int, int> &oraculo)
{
    if (auto v = arbol.violacion())
    {
        std::cerr << "violacion: " << *v << '\n';
        return false;
    }
    if (arbol.tam() != oraculo.size())
        return false;
    // Predicado generico (`auto`): compara un Nodo del arbol con un par del map.
    const auto mismo = [](const auto &nodo, const auto &par)
    { return nodo.clave == par.first && nodo.valor == par.second; };
    return std::ranges::equal(arbol, oraculo, mismo) &&                                   // iterador inorden
           std::ranges::equal(arbol | vistas::reverse, oraculo | vistas::reverse, mismo); // y al reves (operator--)
}

// -------------------------------------------------------------------------------------
// 3. PRUEBAS DE PROPIEDADES
// -------------------------------------------------------------------------------------

// Mezcla original: 60 % inserciones, 40 % eliminaciones, contra el oraculo.
static void prueba_aleatoria(unsigned semilla, int operaciones, int rango_claves)
{
    std::mt19937 gen(semilla); // generador reproducible
    std::uniform_int_distribution<int> clave(0, rango_claves);
    std::uniform_int_distribution<int> moneda(0, 99);
    ArbolRN<int, int> arbol;
    std::map<int, int> oraculo;
    bool todo_bien = true;
    for (int i = 0; i < operaciones && todo_bien; ++i)
    {
        const int k = clave(gen);
        if (moneda(gen) < 60)
        { // 60 % inserciones, 40 % eliminaciones
            const bool nuevo = arbol.insertar(k, i);
            const bool nuevo_oraculo = oraculo.insert_or_assign(k, i).second;
            todo_bien = (nuevo == nuevo_oraculo);
        }
        else
        {
            todo_bien = (arbol.eliminar(k) == (oraculo.erase(k) == 1));
        }
        todo_bien = todo_bien && coincide(arbol, oraculo);
    }
    VERIFICAR(todo_bien);
}

// Mezcla AMPLIADA: ademas ejercita try_emplace, operator[], lower/upper_bound, eliminar(iterador),
// eliminar_si, entre(), copia y asignacion por movimiento; todo contra std::map.
static void prueba_aleatoria_avanzada(unsigned semilla, int operaciones, int rango_claves)
{
    std::mt19937 gen(semilla);
    std::uniform_int_distribution<int> clave(0, rango_claves);
    std::uniform_int_distribution<int> dado(0, 99);
    ArbolRN<int, int> arbol;
    std::map<int, int> oraculo;
    bool todo_bien = true;

    // "Estan en la misma posicion?": ambos al final, o ambos con la misma clave.
    const auto misma_pos = [&](ArbolRN<int, int>::const_iterator ia, std::map<int, int>::const_iterator io)
    {
        const bool a_fin = (ia == arbol.end());
        const bool o_fin = (io == oraculo.end());
        return a_fin == o_fin && (a_fin || ia->clave == io->first);
    };

    for (int i = 0; i < operaciones && todo_bien; ++i)
    {
        const int op = dado(gen);
        const int k = clave(gen);
        if (op < 35)
        { // insertar (o actualizar)
            todo_bien = (arbol.insertar(k, i) == oraculo.insert_or_assign(k, i).second);
        }
        else if (op < 55)
        { // eliminar por clave
            todo_bien = (arbol.eliminar(k) == (oraculo.erase(k) == 1));
        }
        else if (op < 65)
        { // try_emplace: NO sobreescribe
            const auto [nodo, creado] = arbol.try_emplace(k, i);
            const auto [it, creado_o] = oraculo.try_emplace(k, i);
            todo_bien = (creado == creado_o) && nodo->valor == it->second;
        }
        else if (op < 72)
        { // operator[]: crea con 0 si no existe
            arbol[k] += 1;
            oraculo[k] += 1;
        }
        else if (op < 82)
        { // lower_bound y upper_bound
            todo_bien = misma_pos(arbol.lower_bound(k), oraculo.lower_bound(k)) &&
                        misma_pos(arbol.upper_bound(k), oraculo.upper_bound(k));
        }
        else if (op < 90)
        { // eliminar(iterador) devuelve el SIGUIENTE
            const auto it = arbol.lower_bound(k);
            const auto oit = oraculo.lower_bound(k);
            if (it != arbol.end() && oit != oraculo.end())
            {
                const auto siguiente = arbol.eliminar(it);
                const auto o_siguiente = oraculo.erase(oit);
                todo_bien = misma_pos(siguiente, o_siguiente);
            }
        }
        else if (op < 93)
        { // eliminar_si  (== std::erase_if)
            const std::size_t a = arbol.eliminar_si([](int c, int)
                                                    { return c % 7 == 0; });
            const std::size_t b = std::erase_if(oraculo, [](const auto &par)
                                                { return par.first % 7 == 0; });
            todo_bien = (a == b);
        }
        else if (op < 96)
        { // copia profunda + asignacion por movimiento
            ArbolRN<int, int> copia = arbol;
            todo_bien = (copia == arbol) && copia.es_valido();
            arbol = std::move(copia);
        }
        else
        { // entre(a, b) cuenta lo mismo que el map
            const std::ptrdiff_t cuenta = std::ranges::distance(arbol.entre(k, k + 20));
            const std::ptrdiff_t cuenta_o = std::distance(oraculo.lower_bound(k), oraculo.upper_bound(k + 20));
            todo_bien = (cuenta == cuenta_o);
        }
        todo_bien = todo_bien && coincide(arbol, oraculo);
    }
    VERIFICAR(todo_bien);
}

static void prueba_borrar_todo()
{
    ArbolRN<int, int> arbol;
    for (int k = 0; k < 500; ++k)
        arbol.insertar(k, k);
    bool ok = true;
    for (int k = 0; k < 500 && ok; ++k)
    { // borrar en orden creciente
        ok = arbol.eliminar(k) && arbol.es_valido();
    }
    VERIFICAR(ok && arbol.vacio() && arbol.raiz() == nullptr && arbol.altura() == -1);

    for (int k = 0; k < 500; ++k)
        arbol.insertar(k, k);
    for (int k = 499; k >= 0 && ok; --k)
        ok = arbol.eliminar(k) && arbol.es_valido(); // decreciente
    VERIFICAR(ok && arbol.vacio());
}

// -------------------------------------------------------------------------------------
// 2. PRUEBAS DE SEMANTICA
// -------------------------------------------------------------------------------------
static void prueba_semantica()
{
    // Arbol vacio
    ArbolRN<int, int> v;
    VERIFICAR(v.vacio() && v.tam() == 0 && v.minimo() == nullptr && v.maximo() == nullptr);
    VERIFICAR(v.begin() == v.end() && !v.eliminar(1) && !v.contiene(1) && v.es_valido());
    VERIFICAR(v.altura() == -1 && v.lower_bound(5) == v.end() && std::ranges::empty(v.entre(1, 9)));

    // Duplicados: actualiza valor, no crea nodo
    ArbolRN<int, std::string> a;
    VERIFICAR(a.insertar(5, "cinco"));
    VERIFICAR(!a.insertar(5, "CINCO"));
    VERIFICAR(a.tam() == 1 && a.buscar(5)->valor == "CINCO");

    // Claves tipo string
    ArbolRN<std::string, int> s;
    for (const char *t : {"pera", "manzana", "uva", "kiwi", "banana"})
        s.insertar(t, 0);
    VERIFICAR(s.minimo()->clave == "banana" && s.maximo()->clave == "uva" && s.es_valido());

    // Comparador personalizado: orden DESCENDENTE
    ArbolRN<int, int, std::greater<int>> d;
    for (int k : {3, 1, 4, 1, 5, 9, 2, 6})
        d.insertar(k, k);
    int anterior = 1 << 30;
    bool descendente = true;
    for (const auto &n : d)
    {
        descendente = descendente && n.clave < anterior;
        anterior = n.clave;
    }
    VERIFICAR(descendente && d.tam() == 7 && d.es_valido());

    // Comparador = lambda SIN captura (en C++20 se puede construir por defecto): orden por longitud de texto.
    auto por_largo = [](const std::string &x, const std::string &y)
    { return x.size() < y.size(); };
    ArbolRN<std::string, int, decltype(por_largo)> l;
    l.insertar("aaa", 1);
    l.insertar("b", 2);
    l.insertar("cc", 3);
    l.insertar("zzz", 4); // "zzz" ~ "aaa": duplicado
    VERIFICAR(l.tam() == 3 && l.minimo()->clave == "b" && l.es_valido());

    // Comparador CON estado: hay que usar el constructor ArbolRN(Comp)
    struct PorModulo
    {
        int m;
        bool operator()(int x, int y) const { return (x % m) < (y % m); }
    };
    ArbolRN<int, int, PorModulo> mod(PorModulo{10});
    for (int k : {15, 25, 7, 18})
        mod.insertar(k, k); // 15 y 25 son "iguales" (mismo resto)
    VERIFICAR(mod.tam() == 3 && mod.comparador().m == 10 && mod.es_valido());

    // Semantica de movimiento: el original queda vacio y el destino valido
    ArbolRN<int, int> origen;
    for (int k = 0; k < 100; ++k)
        origen.insertar(k, k);
    ArbolRN<int, int> destino(std::move(origen));
    VERIFICAR(origen.vacio() && destino.tam() == 100 && destino.es_valido());
    ArbolRN<int, int> otro;
    otro = std::move(destino);
    VERIFICAR(destino.vacio() && otro.tam() == 100 && otro.es_valido());
    origen.insertar(1, 1); // un arbol "movido" sigue siendo utilizable
    VERIFICAR(origen.tam() == 1 && origen.es_valido());

    // Lista de inicializacion + algoritmos estandar con el iterador (necesita begin/end e iterator_traits)
    ArbolRN<int, int> li{{2, 20}, {1, 10}, {3, 30}};
    VERIFICAR(std::distance(li.begin(), li.end()) == 3);

    // Deduccion de plantilla (CTAD) con la guia de deduccion y construccion desde un rango
    ArbolRN deducido{std::pair{1, std::string("uno")}, std::pair{2, std::string("dos")}};
    VERIFICAR((std::is_same_v<decltype(deducido), ArbolRN<int, std::string>>) && deducido.tam() == 2);
    const std::map<std::string, double> m{{"pi", 3.14159}, {"e", 2.71828}};
    ArbolRN desde_mapa(desde_rango, m);
    VERIFICAR((std::is_same_v<decltype(desde_mapa), ArbolRN<std::string, double>>) && desde_mapa.at("pi") > 3.0);
    const std::vector<std::pair<int, int>> pares{{5, 50}, {3, 30}, {5, 55}};
    ArbolRN<int, int> desde_vector(desde_rango, pares);
    VERIFICAR(desde_vector.tam() == 2 && desde_vector.at(5) == 55);                                     // el segundo 5 actualiza
    VERIFICAR(desde_vector.insertar_varios(std::pair{1, 10}, std::pair{3, 33}, std::pair{9, 90}) == 2); // fold

    // operator[] y at()
    ArbolRN<std::string, int> cuenta;
    ++cuenta["x"];
    ++cuenta["x"];
    ++cuenta["y"];
    bool lanzo = false;
    try
    {
        (void)cuenta.at("z");
    }
    catch (const std::out_of_range &)
    {
        lanzo = true;
    }
    VERIFICAR(cuenta.at("x") == 2 && cuenta.at("y") == 1 && lanzo && cuenta.tam() == 2);

    // valor_de: puntero al valor modificable
    if (int *p = cuenta.valor_de(std::string("x")))
        *p = 42;
    VERIFICAR(cuenta.at("x") == 42 && cuenta.valor_de(std::string("w")) == nullptr);

    // Recorridos: arbol de la Secuencia A = 20(10(.,15), 30(25,.))
    ArbolRN<int, int> sa;
    for (int k : {10, 20, 30, 15, 25})
        sa.insertar(k, k);
    std::vector<int> ino, pre, pos, niveles, nivel_de;
    sa.inorden([&](int k, int)
               { ino.push_back(k); });
    sa.preorden([&](int k, int)
                { pre.push_back(k); });
    sa.postorden([&](int k, int)
                 { pos.push_back(k); });
    sa.por_niveles([&](const auto &nodo, int nivel)
                   { niveles.push_back(nodo.clave); nivel_de.push_back(nivel); });
    VERIFICAR((ino == std::vector<int>{10, 15, 20, 25, 30}) && (pre == std::vector<int>{20, 10, 15, 30, 25}));
    VERIFICAR((pos == std::vector<int>{15, 10, 25, 30, 20}) && (niveles == std::vector<int>{20, 10, 30, 15, 25}));
    VERIFICAR((nivel_de == std::vector<int>{0, 1, 1, 2, 2}));

    // Estadisticas
    const auto est = sa.estadisticas();
    VERIFICAR(est.tam == 5 && est.rojos == 2 && est.negros == 3 && est.altura == 2 && est.altura_negra == 2);
    VERIFICAR(est == sa.estadisticas()); // == por defecto sobre la estructura

    // swap (se encuentra por ADL) y vistas
    ArbolRN<int, int> x{{1, 1}}, y{{2, 2}, {3, 3}};
    swap(x, y);
    VERIFICAR(x.tam() == 2 && y.tam() == 1 && y.minimo()->clave == 1);
    std::vector<int> ks;
    for (int k : sa.claves() | vistas::take(3))
        ks.push_back(k);
    VERIFICAR((ks == std::vector<int>{10, 15, 20}));
    VERIFICAR(std::ranges::max(sa.valores()) == 30);
}

// Tipos de apoyo para las pruebas siguientes -----------------------------------------------

// Un valor cuyo constructor puede lanzar una excepcion (para probar la garantia fuerte).
struct Bomba
{
    explicit Bomba(int n) : valor(n)
    {
        if (n < 0)
            throw std::runtime_error("bomba");
    }
    int valor;
};

// Una clave que CUENTA cuantas veces se construye (para demostrar la busqueda heterogenea).
struct Clave
{
    int v;
    static inline int construcciones = 0;     // `inline` en variable estatica: definida en la propia clase (C++17)
    Clave(int x) : v(x) { ++construcciones; } // implicita a proposito (int -> Clave)
    Clave(const Clave &o) : v(o.v) { ++construcciones; }
    Clave(Clave &&o) noexcept : v(o.v) { ++construcciones; }
    Clave &operator=(const Clave &) = default;
    Clave &operator=(Clave &&) noexcept = default;
};
// Comparador TRANSPARENTE: sabe comparar Clave con int sin convertir.
struct CmpTransparente
{
    using is_transparent = void;
    bool operator()(const Clave &a, const Clave &b) const { return a.v < b.v; }
    bool operator()(const Clave &a, int b) const { return a.v < b; }
    bool operator()(int a, const Clave &b) const { return a < b.v; }
};
// Comparador NO transparente: solo compara Claves (un int tendra que convertirse).
struct CmpSimple
{
    bool operator()(const Clave &a, const Clave &b) const { return a.v < b.v; }
};

static void prueba_busqueda_heterogenea()
{
    ArbolRN<Clave, int, CmpTransparente> t;
    for (int i = 0; i < 200; ++i)
        t.insertar(Clave(i), i);
    Clave::construcciones = 0;
    const auto *n = t.buscar(137);                                            // Q = int: se compara directamente
    VERIFICAR(n != nullptr && n->valor == 137 && Clave::construcciones == 0); // CERO claves temporales
    VERIFICAR(t.contiene(5) && !t.contiene(999) && Clave::construcciones == 0);
    VERIFICAR(t.lower_bound(50)->clave.v == 50 && t.upper_bound(50)->clave.v == 51 && Clave::construcciones == 0);
    VERIFICAR(t.eliminar(10) && !t.contiene(10) && t.es_valido() && Clave::construcciones == 0);

    ArbolRN<Clave, int, CmpSimple> s;
    for (int i = 0; i < 200; ++i)
        s.insertar(Clave(i), i);
    Clave::construcciones = 0;
    VERIFICAR(s.buscar(137) != nullptr);   // Q = int: se convierte a Clave UNA sola vez...
    VERIFICAR(Clave::construcciones == 1); // ...aunque se hagan ~8 comparaciones al bajar

    // Con std::less<> y claves std::string
    ArbolRN<std::string, int, std::less<>> t2;
    for (const char *palabra : {"uno", "dos", "tres", "cuatro"})
        t2.insertar(palabra, 1);
    const std::string_view sv = "tres";
    VERIFICAR(t2.contiene(sv) && t2.contiene("uno") && !t2.contiene(std::string_view{"cinco"}));
    VERIFICAR(*t2.valor_de(sv) == 1 && t2.eliminar(sv) && t2.tam() == 3);
}

static void prueba_copia_y_tipos_especiales()
{
    // ---- Copia profunda: misma forma y colores, y totalmente independiente ----
    std::mt19937 gen(2026);
    std::uniform_int_distribution<int> dado(0, 100000);
    ArbolRN<int, int> original;
    for (int i = 0; i < 300; ++i)
        original.insertar(dado(gen), i);
    ArbolRN<int, int> copia = original;
    std::ostringstream dot_a, dot_b;
    original.exportar_dot(dot_a, true);
    copia.exportar_dot(dot_b, true);
    VERIFICAR(dot_a.str() == dot_b.str()); // el diagrama DOT incluye forma y colores
    VERIFICAR(copia == original && copia.es_valido() && copia.rotaciones() == original.rotaciones());
    copia.eliminar(copia.minimo()->clave);
    copia.insertar(-5, -5);
    VERIFICAR(copia != original && original.es_valido() && original.tam() == 300 && original.minimo()->clave >= 0);

    // Autoasignacion y copia de un arbol vacio
    auto &alias = original;
    original = alias; // `alias` evita el aviso -Wself-assign
    VERIFICAR(original.tam() == 300 && original.es_valido());
    const ArbolRN<int, int> vacio;
    ArbolRN<int, int> copia_vacia = vacio;
    VERIFICAR(copia_vacia.vacio() && copia_vacia == vacio && copia_vacia.es_valido());

    // ---- Valores MOVE-ONLY (unique_ptr): el arbol nunca copia un valor ----
    using SoloMovible = comprobaciones_estaticas::SoloMovible;
    SoloMovible mo;
    for (int k = 0; k < 40; ++k)
        mo.insertar(k, std::make_unique<int>(k * 10));
    mo.try_emplace(100, new int(1000)); // construye el unique_ptr in situ
    VERIFICAR(mo.tam() == 41 && *mo.at(7) == 70 && *mo.at(100) == 1000);
    VERIFICAR(mo.eliminar_si([](int k, const std::unique_ptr<int> &)
                             { return k % 2 == 0; }) == 21 &&
              mo.es_valido());
    SoloMovible movido = std::move(mo);
    VERIFICAR(mo.vacio() && movido.tam() == 20 && movido.es_valido());

    // ---- Garantia FUERTE de excepciones: si construir el valor lanza, el arbol queda intacto ----
    ArbolRN<int, Bomba> b;
    b.try_emplace(1, 5);
    b.try_emplace(2, 6);
    bool lanzo = false;
    try
    {
        b.try_emplace(3, -1);
    }
    catch (const std::runtime_error &)
    {
        lanzo = true;
    }
    VERIFICAR(lanzo && b.tam() == 2 && !b.contiene(3) && b.es_valido());
    VERIFICAR(b.try_emplace(1, -99).second == false); // la clave existe: NO se construye nada (no lanza)

    // ---- try_emplace NO consume el valor cuando la clave ya existe ----
    ArbolRN<int, std::string> t;
    t.insertar(1, "uno");
    std::string valioso(40, 'x');         // largo: no cabe en la optimizacion SSO
    t.try_emplace(1, std::move(valioso)); // existe: el std::move no llega a ejecutarse
    VERIFICAR(valioso.size() == 40 && t.at(1) == "uno");
    t.try_emplace(2, std::move(valioso)); // no existe: ahora si se mueve
    VERIFICAR(valioso.empty() && t.at(2).size() == 40);
}

// -------------------------------------------------------------------------------------
// 4. EXPERIMENTO PARA EL INFORME
// -------------------------------------------------------------------------------------
// Altura de un arbol RN con claves crecientes (el peor caso para un BST).
static void tabla_alturas()
{
    std::cout << "\nAltura (en aristas) tras insertar n claves CRECIENTES\n"
              << "      n | altura RN | cota 2*log2(n+1) | cota entera | bh(raiz) | rotaciones | rot/insercion\n"
              << "--------+-----------+------------------+-------------+----------+------------+--------------\n";
    for (int n : {15, 100, 1000, 10000, 100000, 1000000})
    {
        ArbolRN<int, int> arbol;
        for (int k = 1; k <= n; ++k)
            arbol.insertar(k, k);
        const double cota = 2.0 * std::log2(n + 1.0);
        // setw(ancho) reserva `ancho` columnas y alinea a la derecha; fixed+setprecision fija los decimales.
        std::cout << std::setw(7) << n << " | " << std::setw(9) << arbol.altura() << " | "
                  << std::setw(16) << std::fixed << std::setprecision(1) << cota << " | "
                  << std::setw(11) << cota_altura(static_cast<std::uint64_t>(n)) << " | "
                  << std::setw(8) << arbol.altura_negra(arbol.raiz()) << " | "
                  << std::setw(10) << arbol.rotaciones() << " | "
                  << std::setw(13) << std::setprecision(4) << static_cast<double>(arbol.rotaciones()) / n << '\n';
        VERIFICAR(arbol.es_valido());
        VERIFICAR(arbol.altura() <= cota);        // el teorema: h <= 2 log2(n+1)
        VERIFICAR(arbol.altura_dentro_de_cota()); // lo mismo con la cota entera exacta
        VERIFICAR(cota_altura(static_cast<std::uint64_t>(n)) == static_cast<int>(std::floor(cota)));
    }
    std::cout << std::defaultfloat << std::setprecision(6);
}

int main()
{
    std::cout << "Pruebas de estres del arbol Rojo-Negro (esto puede tardar unos segundos)...\n";
    std::cout << "(las comprobaciones static_assert ya pasaron: si no, este programa no habria compilado)\n";

    prueba_semantica();
    prueba_borrar_todo();
    prueba_busqueda_heterogenea();
    prueba_copia_y_tipos_especiales();

    // Varias semillas y varios "rangos de claves": rango chico = muchos duplicados y borrados exitosos;
    // rango grande = casi todo son inserciones nuevas.
    for (unsigned semilla = 1; semilla <= 20; ++semilla)
        prueba_aleatoria(semilla, 3000, 64);
    for (unsigned semilla = 100; semilla <= 110; ++semilla)
        prueba_aleatoria(semilla, 3000, 100000);
    for (unsigned semilla = 200; semilla <= 215; ++semilla)
        prueba_aleatoria_avanzada(semilla, 2500, 80);
    for (unsigned semilla = 300; semilla <= 305; ++semilla)
        prueba_aleatoria_avanzada(semilla, 2500, 100000);

    tabla_alturas();

    std::cout << "\nVerificaciones ejecutadas: " << g_pruebas << "  |  fallos: " << g_fallos << '\n';
    std::cout << (g_fallos == 0 ? "RESULTADO: TODO OK\n" : "RESULTADO: HAY FALLOS\n");
    return g_fallos == 0 ? 0 : 1;
}
