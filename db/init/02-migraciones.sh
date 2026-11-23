#!/bin/sh
# Aplica las migraciones numeradas del repo de la base (sql/migraciones/), en
# orden, entre el esquema y los datos de ejemplo. Es el mismo orden que usa
# sql/carga/poblar.sh. Como recorre la carpeta, una migración nueva (003-…)
# entra sola la próxima vez que se cree el volumen.
set -e
for migracion in /comandas/migraciones/*.sql; do
    echo "Migración: $(basename "$migracion")"
    psql -X -q -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname "$POSTGRES_DB" -f "$migracion"
done
