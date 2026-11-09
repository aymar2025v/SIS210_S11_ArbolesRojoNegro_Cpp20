// =====================================================================================
//  siga_rn.cpp - Actividad 4 en C++20: SIGA-RN (estudiantes UNAP en un arbol Rojo-Negro)
//  Compilar:  g++ -std=c++20 -Wall -Wextra -Wpedantic -O2 siga_rn.cpp -o siga_rn
//  Muestra: claves std::string, valores struct, busqueda cronometrada con <chrono>,
//           recorrido inorden (lista ordenada por codigo) y diagrama Graphviz, mas:
//             * datos validados en COMPILACION (constexpr + static_assert)
//             * busqueda HETEROGENEA con std::string_view (comparador transparente)
//             * consultas con VISTAS (<ranges>) y baja masiva con eliminar_si
// =====================================================================================
#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "rn.h"

namespace vistas = std::views;

// -------------------------------------------------------------------------------------
// 1. LOS DATOS
// -------------------------------------------------------------------------------------
struct EstudianteUNAP
{
    std::string codigo; // PU-XXXXXXXX
    std::string nombres;
    std::string escuela;
    int ciclo;
    double promedio;
    // `== default` [C++20]: compara campo por campo (util para pruebas y para std::ranges::find).
    bool operator==(const EstudianteUNAP &) const = default;
};

// Version "de tabla" de un estudiante, hecha SOLO con tipos que pueden vivir en tiempo de
// compilacion: std::string_view apunta a los literales (que existen siempre) y no reserva memoria,
// a diferencia de std::string.
struct RegistroEstatico
{
    std::string_view codigo;
    std::string_view nombres;
    std::string_view escuela;
    int ciclo;
    double promedio;
};

// `inline constexpr`: una sola copia en todo el programa, calculada por el compilador.
// (El orden de este arreglo es el orden de matricula; NO se cambia para conservar el arbol del README.)
inline constexpr std::array<RegistroEstatico, 10> REGISTROS{{
    {"PU-20200101", "Mamani Quispe, J.", "Sistemas", 5, 14.2},
    {"PU-20200387", "Huanca Apaza, L.", "Sistemas", 5, 13.8},
    {"PU-20210045", "Ccopa Flores, M.", "IACD", 3, 15.1},
    {"PU-20210312", "Ticona Condori, R.", "Sistemas", 3, 12.9},
    {"PU-20220089", "Cutipa Larico, A.", "IACD", 1, 16.0},
    {"PU-20220154", "Pari Calisaya, Y.", "Sistemas", 1, 11.5},
    {"PU-20180023", "Coyla Mamani, H.", "Sistemas", 9, 15.7},
    {"PU-20190278", "Quispe Tito, B.", "IACD", 7, 14.8},
    {"PU-20230012", "Apaza Cruz, N.", "Sistemas", 1, 13.3},
    {"PU-20230198", "Larico Huanca, C.", "IACD", 1, 12.1},
}};

// Validacion de los datos EN COMPILACION. Una funcion `constexpr` se puede ejecutar dentro del
// compilador; si el resultado es falso, el static_assert detiene la compilacion con un mensaje.
// Asi un codigo mal escrito o repetido se detecta antes de ejecutar el programa.
constexpr bool registros_validos(std::span<const RegistroEstatico> registros)
{
    for (std::size_t i = 0; i < registros.size(); ++i)
    {
        const RegistroEstatico &r = registros[i];
        const bool formato = r.codigo.size() == 11 && r.codigo.starts_with("PU-") &&
                             std::ranges::all_of(r.codigo.substr(3), [](char c)
                                                 { return c >= '0' && c <= '9'; });
        if (!formato)
            return false;
        if (r.promedio < 0.0 || r.promedio > 20.0)
            return false; // escala vigesimal peruana
        if (r.ciclo < 1 || r.ciclo > 10)
            return false;
        for (std::size_t j = i + 1; j < registros.size(); ++j) // sin codigos repetidos
            if (registros[j].codigo == r.codigo)
                return false;
    }
    return true;
}
static_assert(registros_validos(REGISTROS), "hay un codigo mal formado o repetido, o un promedio/ciclo fuera de rango");

// -------------------------------------------------------------------------------------
// 2. UTILIDAD DE MEDICION
// -------------------------------------------------------------------------------------
// Evita que el optimizador elimine un calculo cuyo resultado "nadie usa". La instruccion de
// ensamblador vacia declara que lee `valor` y que toca la memoria, asi que el compilador debe
// producir realmente el valor. (Extension de GCC/Clang; en otros compiladores se usa `volatile`.)
template <typename T>
inline void no_optimizar(const T &valor)
{
#if defined(__GNUC__) || defined(__clang__)
    __asm__ __volatile__("" : : "r,m"(valor) : "memory");
#else
    const volatile T sumidero = valor;
    (void)sumidero;
#endif
}

// -------------------------------------------------------------------------------------
// 3. EL SISTEMA
// -------------------------------------------------------------------------------------
class SigaRN
{
public:
    // `std::less<>` (sin argumento de tipo) es el comparador TRANSPARENTE: permite buscar con un
    // std::string_view o un literal SIN construir un std::string temporal.
    using Arbol = rn::ArbolRN<std::string, EstudianteUNAP, std::less<>>;

    // Matricula a un estudiante. Devuelve false si el codigo YA estaba matriculado (en ese caso
    // NO se pisa el registro existente: por eso se usa try_emplace y no insertar).
    bool matricular(EstudianteUNAP est)
    {
        std::string clave = est.codigo; // copiamos la clave antes de mover `est`
        return arbol_.try_emplace(std::move(clave), std::move(est)).second;
    }

    // Devuelve puntero al estudiante o nullptr; imprime el tiempo de UNA busqueda (ruidoso)
    // y el promedio de muchas (mas confiable). Recibe string_view: vale para std::string, para
    // literales y para porciones de texto, sin copiar nada.
    const EstudianteUNAP *buscar(std::string_view codigo) const
    {
        using reloj = std::chrono::steady_clock;                       // reloj monotono: no retrocede nunca
        using nanosegundos = std::chrono::duration<double, std::nano>; // duracion con decimales

        const auto t0 = reloj::now();
        const auto *nodo = arbol_.buscar(codigo);
        const auto t1 = reloj::now();
        no_optimizar(nodo);
        const auto una_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();

        constexpr int REPETICIONES = 100000;
        std::size_t encontrados = 0; // evita que el compilador elimine el bucle
        const auto r0 = reloj::now();
        for (int i = 0; i < REPETICIONES; ++i)
        {
            const auto *actual = arbol_.buscar(codigo);
            no_optimizar(actual);
            encontrados += (actual != nullptr);
        }
        const auto r1 = reloj::now();
        const double promedio_ns = nanosegundos(r1 - r0).count() / REPETICIONES;

        std::cout << "Busqueda '" << codigo << "': 1 vez = " << una_ns << " ns | promedio de "
                  << REPETICIONES << " = " << std::fixed << std::setprecision(1) << promedio_ns
                  << " ns | altura arbol = " << arbol_.altura() << " (encontrados: " << encontrados / REPETICIONES << ")\n";
        return nodo ? &nodo->valor : nullptr;
    }

    std::vector<EstudianteUNAP> listar_ordenado() const
    {
        std::vector<EstudianteUNAP> resultado;
        resultado.reserve(arbol_.tam());
        arbol_.inorden([&](const std::string &, const EstudianteUNAP &e)
                       { resultado.push_back(e); });
        return resultado;
    }

    // Da de baja a todos los estudiantes que cumplen el criterio. Devuelve cuantos fueron.
    template <typename Pred>
    std::size_t dar_de_baja_si(Pred &&pred)
    {
        return arbol_.eliminar_si([&](const std::string &, const EstudianteUNAP &e)
                                  { return pred(e); });
    }

    const Arbol &arbol() const { return arbol_; }

private:
    Arbol arbol_;
};

int main()
{
    // ---- Cargar los registros (de la tabla constexpr al arbol) ----
    SigaRN siga;
    for (const RegistroEstatico &r : REGISTROS)
        siga.matricular({std::string(r.codigo), std::string(r.nombres), std::string(r.escuela), r.ciclo, r.promedio});

    siga.buscar("PU-20200387");
    siga.buscar("PU-20230198");
    if (siga.buscar("PU-99999999") == nullptr)
        std::cout << "(PU-99999999 no esta matriculado)\n";
    // Matricular dos veces el mismo codigo NO duplica ni pisa el registro:
    const bool repetido = siga.matricular({"PU-20200101", "Otro Nombre", "IACD", 10, 20.0});
    // Se busca ANTES de imprimir: buscar() escribe en std::cout, y si se llamara dentro de la
    // misma expresion `<<` las dos salidas se entremezclarian.
    const EstudianteUNAP *existente = siga.buscar("PU-20200101");
    std::cout << "re-matricular PU-20200101 -> " << std::boolalpha << repetido
              << " (sigue siendo: " << existente->nombres << ")\n";

    std::cout << "\nArbol resultante:\n";
    siga.arbol().imprimir(std::cout);
    std::cout << "tam = " << siga.arbol().tam() << ", altura = " << siga.arbol().altura()
              << ", bh(raiz) = " << siga.arbol().altura_negra(siga.arbol().raiz())
              << ", valido = " << std::boolalpha << siga.arbol().es_valido() << '\n';

    std::cout << "\nLista ordenada:\n";
    for (const auto &e : siga.listar_ordenado())
        std::cout << "  " << e.codigo << " | " << e.nombres << " | ciclo " << e.ciclo << '\n';

    std::ofstream out("arbol_siga.dot");
    siga.arbol().exportar_dot(out, true);
    std::cout << "\n(diagrama Graphviz escrito en arbol_siga.dot)\n";

    // ================================================================================
    // EXTRAS C++20
    // ================================================================================

    // ---- (a) Consultas con VISTAS: se encadenan con `|` y no copian el arbol ----
    // "Los nombres de IACD con promedio >= 14, en orden de codigo":
    std::cout << "\n[Vistas] IACD con promedio >= 14:\n";
    auto destacados_iacd = siga.arbol() | vistas::filter([](const auto &nodo)
                                                         { return nodo.valor.escuela == "IACD" && nodo.valor.promedio >= 14.0; }) |
                           vistas::transform([](const auto &nodo) -> const EstudianteUNAP &
                                             { return nodo.valor; });
    for (const EstudianteUNAP &e : destacados_iacd)
        std::cout << "  " << e.codigo << "  " << std::setw(20) << std::left << e.nombres << std::right << e.promedio << '\n';

    // Rango de codigos (generaciones 2021 y 2022) con entre(): O(log n) para ubicar los extremos.
    std::cout << "\n[entre] codigos desde PU-20210000 hasta PU-20229999:\n";
    for (const auto &nodo : siga.arbol().entre(std::string("PU-20210000"), std::string("PU-20229999")))
        std::cout << "  " << nodo.clave << "  " << nodo.valor.nombres << '\n';

    // ---- (b) Estadisticas por escuela: el arbol es un rango, asi que sirve con std::accumulate ----
    std::map<std::string_view, std::pair<double, int>> por_escuela; // escuela -> (suma de promedios, cantidad)
    for (const auto &nodo : siga.arbol())
    {
        auto &[suma, cantidad] = por_escuela[nodo.valor.escuela]; // enlace estructurado a una referencia
        suma += nodo.valor.promedio;
        ++cantidad;
    }
    std::cout << "\n[Promedios por escuela]\n";
    for (const auto &[escuela, acumulado] : por_escuela)
        std::cout << "  " << std::setw(9) << std::left << escuela << std::right << std::fixed << std::setprecision(2)
                  << acumulado.first / acumulado.second << "  (" << acumulado.second << " estudiantes)\n";
    const double promedio_general = std::accumulate(
                                        siga.arbol().begin(), siga.arbol().end(), 0.0,
                                        [](double acumulado, const auto &nodo)
                                        { return acumulado + nodo.valor.promedio; }) /
                                    static_cast<double>(siga.arbol().tam());
    std::cout << "  promedio general: " << promedio_general << '\n';

    // ---- (c) Ranking: se copia a un vector y se ordena con std::ranges::sort + proyeccion ----
    // Una PROYECCION (&EstudianteUNAP::promedio) le dice a sort "ordena mirando solo este campo".
    std::vector<EstudianteUNAP> ranking = siga.listar_ordenado();
    std::ranges::sort(ranking, std::greater<>{}, &EstudianteUNAP::promedio);
    std::cout << "\n[Top 3 por promedio]\n";
    for (const EstudianteUNAP &e : ranking | vistas::take(3))
        std::cout << "  " << e.promedio << "  " << e.nombres << '\n';

    // ---- (d) Baja masiva con eliminar_si (ahora son seguros los iteradores internos) ----
    const std::size_t bajas = siga.dar_de_baja_si([](const EstudianteUNAP &e)
                                                  { return e.promedio < 12.0; });
    std::cout << "\n[Bajas] promedio < 12.0: " << bajas << " estudiante(s); quedan " << siga.arbol().tam()
              << ", valido = " << std::boolalpha << siga.arbol().es_valido()
              << ", PU-20220154 sigue? " << (siga.arbol().contiene(std::string_view{"PU-20220154"})) << '\n';

    return 0;
}
