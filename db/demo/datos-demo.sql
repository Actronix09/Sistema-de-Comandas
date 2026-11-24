-- =====================================================================
--  datos-demo.sql — La capa de la demo pública sobre los datos de la Práctica 4
--
--  Corre después de 03-poblar.sh, que carga los datos de la Práctica 4 (240
--  eventos, 200 usuarios, el menú del salón y 37 mil comandas), y antes de
--  03-contrasenas-desarrollo.sql, que les pone Demo_123. a todos los usuarios.
--  En lilyth se monta como 03b-datos-demo.sql; la demo se reinicia cada noche.
--
--  Agrega lo que la demo necesita para que cualquiera pueda operar el salón:
--    1. Las cuentas de los botones del login (meseroDemo1, meseroDemo2,
--       cocinaDemo1, cocinaDemo2 en el bar, cajeroDemo y adminDemo).
--    2. El evento en curso de los datos (la posada) pasa a hoy, con sus
--       comandas en horas recientes, y sus primeras mesas a los meseros demo.
--    3. Los dos paquetes que más se piden en ese evento, como incluidos: el
--       mesero ve "Incluido · quedan N" y esas comandas cuestan 0. Necesita la
--       migración 003 (modo_cobro de acuerdo).
--  Todo se busca por estado o por nombre, sin números fijos de los datos.
-- =====================================================================

BEGIN;

-- 1. Usuarios de los botones del login, después de los 200 de la Práctica 4
INSERT INTO usuario (id_usuario, nombre, usuario, fecha_nacimiento, contrasena_hash, rol) VALUES
    (201, 'Mesero Demo 1', 'meseroDemo1', DATE '1998-04-12', 'se-reemplaza-en-04', 'mesero'),
    (202, 'Mesero Demo 2', 'meseroDemo2', DATE '2001-09-30', 'se-reemplaza-en-04', 'mesero'),
    (203, 'Cocina Demo 1', 'cocinaDemo1', DATE '1990-01-05', 'se-reemplaza-en-04', 'bar_cocina'),
    (204, 'Cocina Demo 2', 'cocinaDemo2', DATE '1995-07-21', 'se-reemplaza-en-04', 'bar_cocina'),
    (205, 'Cajero Demo',   'cajeroDemo',  DATE '1985-11-02', 'se-reemplaza-en-04', 'cajero_admin'),
    (206, 'Admin Demo',    'adminDemo',   DATE '1982-03-15', 'se-reemplaza-en-04', 'cajero_admin');
INSERT INTO mesero (id_usuario, turno_asignado) VALUES (201, 'vespertino'), (202, 'nocturno');
-- cocinaDemo2 está en la estación de bar: alguien tiene que preparar las bebidas
INSERT INTO bar_cocina (id_usuario, estacion_asignada) VALUES (203, 'cocina'), (204, 'bar');
INSERT INTO cajero_admin (id_usuario, nivel_acceso, caja_asignada) VALUES (205, 'cajero', 1), (206, 'administrador', 2);

SELECT setval(pg_get_serial_sequence('usuario', 'id_usuario'), (SELECT max(id_usuario) FROM usuario));

-- 2. El evento en curso es de hoy. Sus comandas conservan su orden y sus
-- rondas (las de una misma ronda siguen teniendo la misma hora), pero se
-- acercan al momento en que se crea la base para que cocina las vea recientes.
CREATE TEMP TABLE en_curso ON COMMIT DROP AS
SELECT numero_evento, numero_asistentes FROM evento WHERE estado = 'en_curso';

UPDATE evento SET fecha = CURRENT_DATE WHERE numero_evento IN (SELECT numero_evento FROM en_curso);

UPDATE comanda co
   SET fecha_hora = now() - interval '2 minutes' - (x.ultima - co.fecha_hora) * 0.3
  FROM (SELECT max(fecha_hora) AS ultima FROM comanda
         WHERE numero_evento IN (SELECT numero_evento FROM en_curso)) AS x
 WHERE co.numero_evento IN (SELECT numero_evento FROM en_curso);

-- Las mesas 1 a 3 las atiende Mesero Demo 1 y las 4 a 6 Mesero Demo 2, con las
-- comandas que ya se levantaron en ellas: así ven sus órdenes al entrar.
UPDATE mesa SET id_mesero = CASE WHEN numero_mesa <= 3 THEN 201 ELSE 202 END
 WHERE numero_evento IN (SELECT numero_evento FROM en_curso) AND numero_mesa <= 6;

UPDATE comanda co SET id_mesero = m.id_mesero
  FROM cuenta c JOIN mesa m ON m.numero_evento = c.numero_evento AND m.numero_mesa = c.numero_mesa
 WHERE co.numero_evento = c.numero_evento AND co.secuencial = c.secuencial
   AND c.numero_evento IN (SELECT numero_evento FROM en_curso) AND c.numero_mesa <= 6;

-- 3. Los dos paquetes cuyos productos más se piden en el evento, con una porción
-- por asistente y "al acabarse se cobra"
INSERT INTO acuerdo (numero_evento, numero_paquete, id_producto, cantidad_acordada, modo_cobro)
SELECT e.numero_evento, pp.numero_paquete, pp.id_producto, e.numero_asistentes, 'cobrar'
  FROM en_curso e
  JOIN paquete_producto pp ON pp.numero_paquete IN (
           SELECT pp2.numero_paquete
             FROM paquete_producto pp2
             JOIN comanda co ON co.id_producto = pp2.id_producto
            WHERE co.numero_evento = e.numero_evento AND co.estado <> 'cancelada'
            GROUP BY pp2.numero_paquete
            ORDER BY count(*) DESC, pp2.numero_paquete
            LIMIT 2)
ON CONFLICT DO NOTHING;

-- Las comandas que ya se habían levantado de esos productos se recalculan como
-- incluidas: el precio de una comanda no cambia (tg_comanda_precio), así que
-- el disparador se apaga solo mientras se crea la base, que corre como
-- superusuario. El evento en curso no tiene pagos, así que ningún saldo se
-- descuadra.
ALTER TABLE comanda DISABLE TRIGGER tg_comanda_precio;
UPDATE comanda SET precio_cobrado = 0, tipo_cobro = 'paquete'
 WHERE numero_evento IN (SELECT numero_evento FROM en_curso)
   AND estado <> 'cancelada' AND tipo_cobro = 'normal'
   AND id_producto IN (SELECT a.id_producto FROM acuerdo a
                        WHERE a.numero_evento IN (SELECT numero_evento FROM en_curso) AND a.modo_cobro <> 'aparte');
ALTER TABLE comanda ENABLE TRIGGER tg_comanda_precio;

COMMIT;
