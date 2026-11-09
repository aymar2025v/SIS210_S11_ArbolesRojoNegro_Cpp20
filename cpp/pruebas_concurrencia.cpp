// =====================================================================================
//  pruebas_concurrencia.cpp - ArbolRNSeguro con varios hilos a la vez            [C++20]
//  Compilar (Linux/macOS/MSYS2):  g++ -std=c++20 -Wall -Wextra -Wpedantic -O2 -pthread pruebas_concurrencia.cpp -o concurrencia
//  Ejecutar:  ./concurrencia
//  (MinGW debe usar el modelo de hilos "posix"; si tu g++ dice "win32", usa MSYS2/UCRT64 o WSL.)
//
//  Escenario: 4 ESCRITORES insertan y borran claves en rangos distintos mientras 4 LECTORES
//  buscan sin parar Y validan las 5 propiedades rojo-negro en pleno trafico. Si el cerrojo
//  estuviera mal puesto, un lector veria un arbol a medio rotar: fallaria es_valido() (o
//  ThreadSanitizer, compilando con -fsanitize=thread, avisaria de una carrera de datos).
//
//  Herramientas de C++20 que aparecen:
//    std::jthread    hilo que se une SOLO al destruirse (RAII) y trae un stop_token
//    std::stop_token cancelacion cooperativa: el lector revisa `stop_requested()` en su bucle
//    std::latch      "pistola de salida": todos los hilos esperan y arrancan a la vez
//    std::atomic     contadores compartidos sin cerrojo
// =====================================================================================
#include <atomic>
#include <chrono>
#include <iostream>
#include <latch>
#include <stop_token>
#include <thread>
#include <vector>

#include "rn_seguro.h"

int main()
{
    constexpr int ESCRITORES = 4;
    constexpr int LECTORES = 4;
    constexpr int CLAVES_POR_ESCRITOR = 20000;

    rn::ArbolRNSeguro<int, int> arbol;
    std::latch salida(ESCRITORES + LECTORES); // se abre cuando los 8 hilos llamaron arrive_and_wait()
    std::atomic<long> consultas{0};
    std::atomic<long> arboles_invalidos{0};     // debe terminar en 0
    std::atomic<long> lecturas_incoherentes{0}; // debe terminar en 0

    {
        std::vector<std::jthread> lectores;
        for (int id = 0; id < LECTORES; ++id)
        {
            // El primer parametro std::stop_token lo rellena jthread automaticamente.
            lectores.emplace_back([&, id](std::stop_token parar)
                                  {
                salida.arrive_and_wait();
                long mias = 0;
                while (!parar.stop_requested()) {
                    const int k = static_cast<int>((mias * 7919 + id * 104729) % (ESCRITORES * CLAVES_POR_ESCRITOR));
                    // obtener() devuelve una COPIA del valor: nunca un puntero al interior del arbol.
                    if (const auto v = arbol.obtener(k); v && *v != k) ++lecturas_incoherentes;  // el valor siempre es la propia clave
                    if (mias % 100 == 0) {
                        // Con el cerrojo compartido tomado, el arbol que ve la lambda es CONSISTENTE.
                        const bool ok = arbol.con_lectura([](const auto& a) { return a.es_valido(); });
                        if (!ok) ++arboles_invalidos;
                    }
                    ++mias;
                    // PAUSA OBLIGATORIA: el std::shared_mutex de glibc/pthreads da PREFERENCIA A LOS
                    // LECTORES. Si los lectores consultan sin descanso, siempre hay alguno con el
                    // cerrojo compartido y el escritor NUNCA entra (inanicion de escritores: el
                    // programa parece colgado). Un respiro de unos microsegundos lo evita.
                    std::this_thread::sleep_for(std::chrono::microseconds(20));
                }
                consultas += mias; });
        }

        {
            std::vector<std::jthread> escritores;
            for (int id = 0; id < ESCRITORES; ++id)
            {
                escritores.emplace_back([&, id]
                                        {
                                            salida.arrive_and_wait();
                                            const int base = id * CLAVES_POR_ESCRITOR; // rangos disjuntos: no se pisan
                                            for (int i = 0; i < CLAVES_POR_ESCRITOR; ++i)
                                                arbol.insertar(base + i, base + i);
                                            for (int i = 0; i < CLAVES_POR_ESCRITOR; i += 2)
                                                arbol.eliminar(base + i); // borra las pares
                                        });
            }
        } // <- aqui los escritores se UNEN (join) automaticamente al destruirse el vector

        // Ya terminaron los escritores; los lectores siguen. Se les pide parar al salir del bloque:
        // la destruccion de un std::jthread hace request_stop() + join().
    }

    const std::size_t esperado = static_cast<std::size_t>(ESCRITORES) * (CLAVES_POR_ESCRITOR / 2);
    const bool final_valido = arbol.con_lectura([](const auto &a)
                                                { return a.es_valido(); });
    const bool tam_ok = (arbol.tam() == esperado);
    const auto copia = arbol.instantanea(); // foto consistente (copia profunda), recorrible sin cerrojo
    const bool copia_ok = copia.tam() == esperado && copia.es_valido();

    std::cout << "consultas de los lectores : " << consultas.load() << '\n'
              << "arboles invalidos vistos  : " << arboles_invalidos.load() << "   (esperado 0)\n"
              << "lecturas incoherentes     : " << lecturas_incoherentes.load() << "   (esperado 0)\n"
              << "tamano final              : " << arbol.tam() << "   (esperado " << esperado << ")\n"
              << "arbol final valido        : " << std::boolalpha << final_valido << '\n'
              << "instantanea valida        : " << copia_ok << '\n';

    const bool ok = arboles_invalidos == 0 && lecturas_incoherentes == 0 && tam_ok && final_valido && copia_ok;
    std::cout << (ok ? "RESULTADO: TODO OK\n" : "RESULTADO: HAY FALLOS\n");
    return ok ? 0 : 1;
}
