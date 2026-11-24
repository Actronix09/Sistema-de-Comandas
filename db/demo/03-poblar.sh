#!/bin/sh
# Carga en la base de la demo los datos de la Práctica 4: los mismos CSV y el
# mismo sql/carga/poblar.sql del repositorio de la base, unos 62 mil renglones
# (240 eventos, 200 usuarios, el menú del salón, 37 mil comandas…).
#
# Corre al crear la base, entre las migraciones y db/demo/datos-demo.sql. Hace
# lo que hace poblar.sh, pero dentro del contenedor: copia los CSV a
# /tmp/poblar/datos, que es donde los busca poblar.sql, y lo corre.
#
# Necesita montados:
#   sql/datos/generados  → /comandas/datos
#   sql/carga/poblar.sql → /comandas/carga/poblar.sql
set -e
rm -rf /tmp/poblar
mkdir -p /tmp/poblar
cp -r /comandas/datos /tmp/poblar/datos
echo "Cargando los datos de la Práctica 4…"
psql -X -q -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname "$POSTGRES_DB" -f /comandas/carga/poblar.sql
rm -rf /tmp/poblar
