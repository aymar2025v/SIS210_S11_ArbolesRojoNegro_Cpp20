// =====================================================================================
//  rn_seguro.h  -  ArbolRNSeguro: el arbol rojo-negro usable desde VARIOS HILOS   [C++20]
//  SIS210 - Semana 11 | requiere rn.h y un compilador con std::shared_mutex (-pthread en Linux)
// =====================================================================================
//
//  POR QUE HACE FALTA?
//  ArbolRN, como todos los contenedores de la biblioteca estandar, NO es seguro entre hilos:
//  si un hilo inserta (y rota nodos) mientras otro busca, el lector puede seguir un puntero
//  que justo se esta reasignando => comportamiento indefinido (data race).
//
//  LA SOLUCION: COMPOSICION + CERROJO LECTOR/ESCRITOR
//  Se envuelve un ArbolRN junto a un `std::shared_mutex`:
//    * LECTURAS  (buscar, tam, recorrer)  -> `std::shared_lock`: MUCHOS lectores a la vez.
//    * ESCRITURAS (insertar, eliminar)    -> `std::unique_lock`: UN solo escritor, sin lectores.
//  Los cerrojos son RAII: se sueltan solos al salir del bloque, incluso si hay una excepcion.
//
//  REGLA DE ORO DE ESTE DISENO: NUNCA se devuelve un puntero ni una referencia a algo interno,
//  porque el cerrojo se suelta al retornar y el puntero quedaria apuntando a un nodo que otro
//  hilo podria borrar. Por eso `obtener` devuelve una COPIA del valor (std::optional<V>), y
//  para operaciones mas grandes existe `con_lectura(f)` / `con_escritura(f)`, que ejecutan tu
//  lambda con el cerrojo tomado y devuelven un resultado POR VALOR.
// =====================================================================================
#pragma once

#include <concepts>
#include <functional> // std::invoke
#include <mutex>      // std::unique_lock
#include <optional>
#include <shared_mutex> // std::shared_mutex, std::shared_lock
#include <type_traits>
#include <utility>

#include "rn.h"

namespace rn
{

    template <typename K, typename V, ComparadorDe<K> Comp = std::less<K>>
    class ArbolRNSeguro
    {
    public:
        using Arbol = ArbolRN<K, V, Comp>;

        ArbolRNSeguro() = default;
        explicit ArbolRNSeguro(Comp comp) : arbol_(std::move(comp)) {}

        // El mutex no se puede copiar ni mover, asi que el envoltorio tampoco (se deduce solo,
        // pero lo escribimos para que quede explicito).
        ArbolRNSeguro(const ArbolRNSeguro &) = delete;
        ArbolRNSeguro &operator=(const ArbolRNSeguro &) = delete;

        // ---- Escrituras: cerrojo EXCLUSIVO ----
        bool insertar(K clave, V valor)
        {
            std::unique_lock cerrojo(mutex_); // CTAD: deduce std::unique_lock<std::shared_mutex>
            return arbol_.insertar(std::move(clave), std::move(valor));
        }

        bool eliminar(const K &clave)
        {
            std::unique_lock cerrojo(mutex_);
            return arbol_.eliminar(clave);
        }

        // ---- Lecturas: cerrojo COMPARTIDO (varios hilos a la vez) ----
        [[nodiscard]] bool contiene(const K &clave) const
        {
            std::shared_lock cerrojo(mutex_);
            return arbol_.contiene(clave);
        }

        // Devuelve una COPIA del valor (o nullopt). Nunca un puntero: ver la regla de oro arriba.
        [[nodiscard]] std::optional<V> obtener(const K &clave) const
            requires std::copy_constructible<V>
        {
            std::shared_lock cerrojo(mutex_);
            if (const auto *nodo = arbol_.buscar(clave))
                return nodo->valor;
            return std::nullopt;
        }

        [[nodiscard]] std::size_t tam() const
        {
            std::shared_lock cerrojo(mutex_);
            return arbol_.tam();
        }

        // Ejecuta `f(const Arbol&)` con el cerrojo compartido tomado. `f` ve un arbol CONSISTENTE
        // (ningun escritor puede cambiarlo mientras `f` corre). El resultado debe devolverse por valor.
        template <typename F>
            requires std::invocable<F, const Arbol &>
        auto con_lectura(F &&f) const -> std::invoke_result_t<F, const Arbol &>
        {
            static_assert(!std::is_reference_v<std::invoke_result_t<F, const Arbol &>>,
                          "con_lectura no puede devolver una referencia: apuntaria a datos sin proteger");
            std::shared_lock cerrojo(mutex_);
            return std::invoke(std::forward<F>(f), arbol_);
        }

        // Ejecuta `f(Arbol&)` con el cerrojo exclusivo: varias operaciones juntas como UNA sola
        // operacion atomica (por ejemplo "si no existe, insertar y contar").
        template <typename F>
            requires std::invocable<F, Arbol &>
        auto con_escritura(F &&f) -> std::invoke_result_t<F, Arbol &>
        {
            static_assert(!std::is_reference_v<std::invoke_result_t<F, Arbol &>>,
                          "con_escritura no puede devolver una referencia: apuntaria a datos sin proteger");
            std::unique_lock cerrojo(mutex_);
            return std::invoke(std::forward<F>(f), arbol_);
        }

        // FOTO consistente: una COPIA PROFUNDA del arbol hecha bajo el cerrojo compartido. Luego se
        // puede recorrer la copia con calma, sin cerrojo y sin bloquear a los escritores.
        // (Usa el constructor de copia de ArbolRN, que solo existe si K y V se pueden copiar.)
        [[nodiscard]] Arbol instantanea() const
            requires std::copy_constructible<Arbol>
        {
            std::shared_lock cerrojo(mutex_);
            return arbol_;
        }

    private:
        // `mutable`: tomar un cerrojo "modifica" el mutex, pero los metodos de LECTURA son const.
        mutable std::shared_mutex mutex_;
        Arbol arbol_;
    };

} // namespace rn
