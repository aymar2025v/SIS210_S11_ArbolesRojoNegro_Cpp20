// =====================================================================================
//  rn.cpp  -  Pruebas de la guia (Semana 11) + secuencias A y B con trazas paso a paso
//             + demostracion de las capacidades C++20 de rn.h (rangos, vistas, <=>, ...)
//  Compilar:  g++ -std=c++20 -Wall -Wextra -Wpedantic -O2 rn.cpp -o rn
//  Ejecutar:  ./rn          (en Windows: .\rn.exe)
//  IMPORTANTE: no compilar con -DNDEBUG, porque eso desactiva los assert de las pruebas.
// =====================================================================================
#include <cassert>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <ranges>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "rn.h"

using namespace rn; // solo en un .cpp de pruebas; en un header NUNCA se hace esto
using ArbolInt = ArbolRN<int, int>;
namespace vistas = std::views; // alias de espacio de nombres: `vistas::filter` en vez de `std::views::filter`

// ---- utilidades de presentacion -----------------------------------------------------

static void titulo(const std::string &texto)
{
    std::cout << "\n==== " << texto << " ====\n";
}

// "Caso 2 (triangulo) [rotar_izquierda] + Caso 3 (linea) [rotar_derecha]"  o  "-"
// Recibe un std::span: sirve con un vector, un arreglo o lo que devuelve arbol.eventos(), sin copiar.
static std::string describir(std::span<const EventoFixup> eventos)
{
    if (eventos.empty())
        return "- (sin fixup)";
    std::string s;
    for (const EventoFixup &e : eventos)
    {
        if (!s.empty())
            s += " + ";
        s += nombre(e.caso);
        if (e.rotacion != Rotacion::NINGUNA)
        {
            s += " [";
            s += nombre(e.rotacion);
            s += "]";
        }
    }
    return s;
}

// Inserta una secuencia mostrando el arbol DESPUES de cada insercion y su fixup.
static void insertar_con_traza(ArbolInt &arbol, const std::vector<int> &claves)
{
    arbol.registrar_eventos(true);
    for (int k : claves)
    {
        arbol.insertar(k, k);
        std::cout << "\n-- insertar " << k << " --> " << describir(arbol.eventos()) << "\n";
        arbol.imprimir(std::cout);
        // El validador comprueba P2, P4, P5, orden BST y punteros padre despues de CADA paso.
        // exigir_valido() aborta indicando ESTA linea si algo falla (y no depende de NDEBUG).
        arbol.exigir_valido();
    }
}

static void guardar_dot(const ArbolInt &arbol, const std::string &archivo)
{
    std::ofstream out(archivo);
    arbol.exportar_dot(out, /*incluir_nil=*/true);
    std::cout << "(diagrama Graphviz escrito en " << archivo << ")\n";
}

int main()
{
    // ================================================================================
    // PRUEBAS DE LA GUIA, Actividad 1.1 (verificacion en C++ de la Secuencia A)
    // ================================================================================
    titulo("Actividad 1.1 - Secuencia A: 10, 20, 30, 15, 25");
    {
        ArbolInt arbol;
        insertar_con_traza(arbol, {10, 20, 30, 15, 25});

        assert(arbol.raiz()->color == Color::NEGRO);   // P2
        assert(arbol.altura_negra(arbol.raiz()) == 2); // bh = 2
        assert(arbol.buscar(15) != nullptr);
        // Forma esperada (la que realmente resulta; ver README, nota 1 sobre la guia):
        assert(arbol.raiz()->clave == 20);
        assert(arbol.raiz()->izq->clave == 10 && arbol.raiz()->izq->color == Color::NEGRO);
        assert(arbol.raiz()->der->clave == 30 && arbol.raiz()->der->color == Color::NEGRO);
        assert(arbol.raiz()->izq->der->clave == 15 && arbol.raiz()->izq->der->color == Color::ROJO);
        assert(arbol.raiz()->der->izq->clave == 25 && arbol.raiz()->der->izq->color == Color::ROJO);
        std::cout << "\nSecuencia A: OK  (altura = " << arbol.altura()
                  << ", bh(raiz) = " << arbol.altura_negra(arbol.raiz()) << ")\n";
        guardar_dot(arbol, "arbol_secuencia_A.dot");
    }

    // ================================================================================
    // Actividad 1.2 - Secuencia B, con la tabla que pide la guia
    // ================================================================================
    titulo("Actividad 1.2 - Secuencia B: 7, 3, 18, 10, 22, 8, 11, 26");
    {
        ArbolInt arbol;
        arbol.registrar_eventos(true);
        std::cout << "Paso | Clave | " << std::left << std::setw(41) << "Caso fixup / rotacion"
                  << "| Estado raiz\n"
                  << "-----+-------+" << std::string(42, '-') << "+------------\n";
        int paso = 1;
        std::size_t casos1 = 0;
        for (int k : {7, 3, 18, 10, 22, 8, 11, 26})
        {
            arbol.insertar(k, k);
            // std::setw / std::left / std::right: manipuladores de <iomanip> para alinear columnas.
            std::cout << std::right << std::setw(4) << paso++ << " | " << std::setw(5) << k << " | "
                      << std::left << std::setw(41) << describir(arbol.eventos()) << "| "
                      << arbol.raiz()->clave << '(' << arbol.raiz()->color << ")\n";
            // Con `operator==` por defecto en EventoFixup se compara un evento entero de una vez:
            for (const EventoFixup &e : arbol.eventos())
                casos1 += (e == EventoFixup{CasoFixup::INS_CASO1_TIO_ROJO, Rotacion::NINGUNA});
            arbol.exigir_valido();
        }
        std::cout << std::right << "\nArbol final:\n";
        arbol.imprimir(std::cout);
        std::cout << "rotaciones totales = " << arbol.rotaciones()
                  << "  (la Secuencia B solo usa el Caso 1: no hay rotaciones)\n";
        assert(arbol.rotaciones() == 0);
        // El contador por caso lo confirma: solo hubo Caso 1 (dos veces).
        assert(arbol.conteo(CasoFixup::INS_CASO1_TIO_ROJO) == 2 && casos1 == 2);
        assert(arbol.conteo(CasoFixup::INS_CASO2_TRIANGULO) == 0 && arbol.conteo(CasoFixup::INS_CASO3_LINEA) == 0);
        guardar_dot(arbol, "arbol_secuencia_B.dot");
    }

    // ================================================================================
    // Actividad 2.1 - Casos de prueba obligatorios (copiados de la guia)
    // ================================================================================
    titulo("Actividad 2.1 - Casos de prueba obligatorios P1..P4");
    {
        // P1: insercion simple - no viola propiedades
        ArbolInt arbol;
        arbol.insertar(10, 10);
        assert(arbol.raiz()->color == Color::NEGRO);
        std::cout << "P1 OK\n";

        // P2: insertar 10, 20, 30 -> la raiz pasa a ser 20.
        // (Ojo: aqui el tio es NIL = negro, asi que es una ROTACION (Caso 3), no el Caso 1.)
        ArbolInt a2;
        a2.registrar_eventos(true);
        for (int k : {10, 20, 30})
            a2.insertar(k, k);
        assert(a2.raiz()->clave == 20);
        assert(a2.altura_negra(a2.raiz()) == 1); // bh = 1
        std::cout << "P2 OK   (ultima insercion: " << describir(a2.eventos()) << ")\n";

        // P3: Caso 2+3 (LR) - doble rotacion
        ArbolInt a3;
        a3.registrar_eventos(true);
        for (int k : {30, 10, 20})
            a3.insertar(k, k);
        assert(a3.raiz()->clave == 20);
        assert(a3.raiz()->color == Color::NEGRO);
        std::cout << "P3 OK   (ultima insercion: " << describir(a3.eventos()) << ")\n";

        // P4: consistencia de la altura negra tras 15 inserciones crecientes
        ArbolInt a4;
        for (int k = 1; k <= 15; k++)
            a4.insertar(k, k);
        assert(a4.altura_negra(a4.raiz()) >= 0); // -1 = inconsistente
        assert(a4.raiz()->color == Color::NEGRO);
        assert(a4.es_valido());
        std::cout << "P4 OK   (bh = " << a4.altura_negra(a4.raiz()) << ", altura = " << a4.altura() << ")\n";
        a4.imprimir(std::cout);
        guardar_dot(a4, "arbol_1_a_15.dot");
    }

    // ================================================================================
    // Ampliacion: un Caso 1 de verdad, y eliminacion con sus casos
    // ================================================================================
    titulo("Ampliacion - Caso 1 real: insertar 15 en {10, 20, 30}");
    {
        ArbolInt arbol;
        insertar_con_traza(arbol, {10, 20, 30, 15});
    }

    titulo("Ampliacion - Eliminacion sobre el arbol 1..15");
    {
        ArbolInt arbol;
        for (int k = 1; k <= 15; k++)
            arbol.insertar(k, k);
        arbol.registrar_eventos(true);
        for (int k : {1, 8, 4, 15, 10})
        {
            const bool existia = arbol.eliminar(k);
            std::cout << "\n-- eliminar " << k << " (" << (existia ? "existia" : "no existia") << ") --> "
                      << describir(arbol.eventos()) << "\n";
            arbol.imprimir(std::cout);
            arbol.exigir_valido();
        }
        assert(!arbol.eliminar(999)); // eliminar algo que no esta devuelve false
    }

    titulo("Ampliacion - Recorrido con iterador (inorden)");
    {
        ArbolRN<int, std::string> a{{30, "treinta"}, {10, "diez"}, {20, "veinte"}, {40, "cuarenta"}};
        for (const auto &nodo : a)
            std::cout << nodo.clave << " -> " << nodo.valor << '\n';
        a.insertar(20, "VEINTE"); // clave repetida: solo actualiza el valor
        std::cout << "tras re-insertar 20: tam = " << a.tam() << ", valor = " << a.buscar(20)->valor << '\n';
    }

    // ================================================================================
    // AMPLIACION C++20 (nueva): lo que el arbol ofrece gracias a concepts y rangos
    // ================================================================================
    titulo("C++20 - Rangos y vistas: el arbol es un std::ranges::bidirectional_range");
    {
        ArbolInt arbol;
        for (int k = 1; k <= 15; ++k)
            arbol.insertar(k, k * k);

        // Una "tuberia" de vistas con `|`: NO se copia nada ni se crea ningun vector intermedio;
        // cada elemento fluye por el filtro y por el reverse cuando el `for` lo pide (evaluacion perezosa).
        auto pares_al_reves = arbol.claves() | vistas::filter([](int k)
                                                              { return k % 2 == 0; }) |
                              vistas::reverse;
        std::cout << "claves pares de mayor a menor : ";
        std::vector<int> obtenidas;
        for (int k : pares_al_reves)
        {
            std::cout << k << ' ';
            obtenidas.push_back(k);
        }
        std::cout << '\n';
        assert((obtenidas == std::vector<int>{14, 12, 10, 8, 6, 4, 2}));

        // entre(a, b): subrango [a, b] localizado con dos busquedas O(log n).
        std::cout << "entre(5, 9) con su valor      : ";
        for (const auto &n : arbol.entre(5, 9))
            std::cout << n.clave << "->" << n.valor << ' ';
        std::cout << '\n';
        assert(std::ranges::distance(arbol.entre(5, 9)) == 5);
        assert(std::ranges::distance(arbol.entre(9, 5)) == 0); // intervalo invertido => vacio

        // Iterador bidireccional: --end() es el maximo; lower_bound/upper_bound como en std::map.
        auto it = arbol.end();
        --it;
        std::cout << "--end() apunta a              : " << it->clave << '\n';
        assert(it->clave == 15 && arbol.lower_bound(100) == arbol.end());
        assert(arbol.lower_bound(7)->clave == 7 && arbol.upper_bound(7)->clave == 8);

        // Los algoritmos de <ranges> funcionan directamente sobre el arbol.
        const auto rojos = std::ranges::count_if(arbol, [](const auto &n)
                                                 { return n.color == Color::ROJO; });
        std::cout << "nodos rojos (count_if)        : " << rojos << " de " << arbol.size() << '\n';

        // Y el recorrido inverso con rbegin()/rend():
        std::cout << "rbegin..rend                  : ";
        for (auto r = arbol.rbegin(); r != arbol.rend(); ++r)
            std::cout << r->clave << ' ';
        std::cout << '\n';
    }

    titulo("C++20 - try_emplace, operator[], at() y busqueda con claves de otro tipo");
    {
        ArbolRN<std::string, int> conteo;
        for (const char *palabra : {"rojo", "negro", "rojo", "rojo", "negro", "hoja"})
            ++conteo[palabra]; // operator[] crea la clave con valor 0 si no existe y devuelve una referencia
        for (const auto &n : conteo)
            std::cout << "  " << n.clave << ": " << n.valor << '\n';
        assert(conteo.at("rojo") == 3 && conteo.at("negro") == 2 && conteo.at("hoja") == 1);

        const auto [nodo1, creado1] = conteo.try_emplace("raiz", 7);  // no existia: se crea
        const auto [nodo2, creado2] = conteo.try_emplace("raiz", 99); // existia: NO se toca
        assert(creado1 && !creado2 && nodo1 == nodo2 && nodo2->valor == 7);

        try
        {
            (void)conteo.at("no existe");
            assert(false); // no debe llegar aqui
        }
        catch (const std::out_of_range &e)
        {
            std::cout << "at(\"no existe\") lanzo: " << e.what() << '\n';
        }

        // Con un comparador transparente (std::less<>), buscar con string_view NO construye un std::string.
        ArbolRN<std::string, int, std::less<>> transparente{{"alfa", 1}, {"beta", 2}, {"gamma", 3}};
        const std::string_view buscado = "beta";
        assert(transparente.buscar(buscado) != nullptr && transparente.buscar(buscado)->valor == 2);
        assert(transparente.contiene("gamma") && !transparente.contiene(std::string_view{"delta"}));
        std::cout << "busqueda heterogenea (string_view) en un arbol de std::string: OK\n";
    }

    titulo("C++20 - Comparar arboles por CONTENIDO con == y <=>");
    {
        ArbolInt a, b;
        for (int k : {1, 2, 3, 4, 5, 6, 7})
            a.insertar(k, k);
        for (int k : {7, 6, 5, 4, 3, 2, 1})
            b.insertar(k, k);

        std::ostringstream forma_a, forma_b;
        a.exportar_dot(forma_a);
        b.exportar_dot(forma_b);
        std::cout << "misma forma (diagrama DOT identico)? " << std::boolalpha << (forma_a.str() == forma_b.str())
                  << "   | mismo contenido (==)? " << (a == b) << '\n';
        assert(a == b); // mismo contenido, aunque la forma pueda diferir

        ArbolInt c = a; // copia PROFUNDA (nodo por nodo, misma forma y colores)
        c.insertar(100, 100);
        assert(a != c && a < c); // `a` es un prefijo de `c` => a < c  (orden lexicografico)
        assert(a.es_valido() && c.es_valido() && a.tam() == 7 && c.tam() == 8);

        ArbolInt d;
        d = c; // asignacion por copia
        d.eliminar(100);
        assert(d == a && c.tam() == 8); // d volvio a ser igual a `a`; c no cambio
        std::cout << "copia profunda y asignacion por copia: OK\n";
    }

    titulo("C++20 - Estadisticas, contadores por caso y eliminar_si");
    {
        ArbolInt arbol;
        for (int k = 1; k <= 1000; ++k)
            arbol.insertar(k, k);
        std::cout << "Tras insertar 1..1000 (claves crecientes):\n";
        for (std::size_t i = 0; i < NUM_CASOS; ++i)
        {
            const auto caso = static_cast<CasoFixup>(i);
            std::cout << "  " << std::left << std::setw(38) << nombre(caso) << std::right << std::setw(6)
                      << arbol.conteo(caso) << " veces\n";
        }
        const std::size_t borrados = arbol.eliminar_si([](int clave, int)
                                                       { return clave % 2 == 0; });
        std::cout << "eliminar_si(clave par) borro " << borrados << " elementos; quedan " << arbol.tam() << '\n';
        assert(borrados == 500 && arbol.tam() == 500 && arbol.es_valido());
        for (std::size_t i = 3; i < NUM_CASOS; ++i)
        {
            const auto caso = static_cast<CasoFixup>(i);
            std::cout << "  " << std::left << std::setw(38) << nombre(caso) << std::right << std::setw(6)
                      << arbol.conteo(caso) << " veces\n";
        }

        const auto e = arbol.estadisticas();
        std::cout << "estadisticas: tam=" << e.tam << " rojos=" << e.rojos << " negros=" << e.negros
                  << " altura=" << e.altura << " bh=" << e.altura_negra << " rotaciones=" << e.rotaciones << '\n';
        assert(e.rojos + e.negros == e.tam && arbol.altura_dentro_de_cota());
    }

    titulo("C++20 - std::format con tipos propios (si tu compilador lo trae)");
    {
#if RN_TIENE_FORMAT
        std::cout << std::format("[{:>6}] [{:<6}] [{:^34}]\n", Color::ROJO, Color::NEGRO, CasoFixup::INS_CASO3_LINEA);
#else
        std::cout << "(tu libstdc++ no trae <format>; se omite esta demostracion)\n";
#endif
    }

    std::cout << "\nTodas las pruebas de rn.cpp pasaron.\n";
    return 0;
}
