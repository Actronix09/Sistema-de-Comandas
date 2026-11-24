-- =====================================================================
--  datos-demo.sql — Lo que se agrega a los datos de ejemplo en la demo pública
--
--  Corre después de datos-ejemplo.sql (del repo de la base) y antes de
--  03-contrasenas-desarrollo.sql, que les pone Demo_123. a todos. En lilyth se
--  monta como 03b-datos-demo.sql; la demo se reinicia cada noche desde cero.
--
--  Deja el salón como lo mostraba la Api simulada: cada botón de acceso
--  rápido del login tiene su usuario (dcampos administra y mrivas cobra), más
--  mesas, el menú completo con sus recetas y comandas en todos los estados.
--  Las horas se cuentan desde que se crea la base, para que cocina las vea
--  recientes.
-- =====================================================================

BEGIN;

-- Usuarios: la administradora y el cajero de los botones del login
INSERT INTO usuario (id_usuario, nombre, usuario, fecha_nacimiento, contrasena_hash, rol) VALUES
    (6, 'Daniela Campos Vega', 'dcampos', DATE '1982-03-15', 'se-reemplaza-en-04', 'cajero_admin');
INSERT INTO cajero_admin (id_usuario, nivel_acceso, caja_asignada) VALUES (6, 'administrador', 2);
UPDATE cajero_admin SET nivel_acceso = 'cajero' WHERE id_usuario = 5;

-- Inventario. La arrachera y el queso crema quedan por debajo de su mínimo
INSERT INTO ingrediente (id_ingrediente, nombre, unidad_medida, stock_actual, stock_minimo) VALUES
    (9,  'Agua mineral 600 ml',    'pieza',   60,   24),
    (10, 'Cerveza clara 355 ml',   'pieza',   96,   24),
    (11, 'Tequila blanco',         'ml',    3000,  750),
    (12, 'Limón',                  'pieza',  200,   50),
    (13, 'Lechuga romana',         'g',     4000, 1000),
    (14, 'Pechuga de pollo',       'g',     6000, 2000),
    (15, 'Carne de arrachera',     'g',     1500, 2000),
    (16, 'Café molido',            'g',     2000,  500),
    (17, 'Leche',                  'ml',    8000, 2000),
    (18, 'Huevo',                  'pieza',   60,   24),
    (19, 'Azúcar',                 'g',     5000, 1000),
    (20, 'Queso crema',            'g',      600, 1000),
    (21, 'Sal',                    'g',     2000,  500);

-- El resto del menú. Café con leche y la pasta con pollo son variantes
INSERT INTO producto (id_producto, nombre, descripcion, categoria, precio, id_producto_principal) VALUES
    (5,  'Agua mineral',            'Botella de 600 ml',                                   'bebidas',    30.00, NULL),
    (6,  'Cerveza',                 'Cerveza clara de 355 ml',                             'bebidas',    60.00, NULL),
    (7,  'Margarita',               'Tequila, jugo de limón y escarchado de sal',          'bebidas',   110.00, NULL),
    (8,  'Café americano',          'Café de grano recién preparado',                      'bebidas',    40.00, NULL),
    (9,  'Café con leche',          'Café americano con leche caliente',                   'bebidas',    45.00, 8),
    (10, 'Ensalada César',          'Lechuga romana, pollo a la parrilla y aderezo César', 'alimentos', 140.00, NULL),
    (11, 'Arrachera',               '250 g de arrachera a la parrilla con guarnición',     'alimentos', 260.00, NULL),
    (12, 'Pasta Alfredo con pollo', 'La pasta Alfredo de la casa con pollo a la parrilla', 'alimentos', 210.00, 3),
    (13, 'Flan napolitano',         'Flan casero con caramelo',                            'dulceria',   70.00, NULL),
    (14, 'Pay de queso',            'Rebanada de pay de queso con base de galleta',        'dulceria',   80.00, NULL);

INSERT INTO receta (id_producto, id_ingrediente, cantidad_por_porcion) VALUES
    (5, 9, 1),
    (6, 10, 1),
    (7, 11, 60), (7, 12, 2), (7, 3, 150), (7, 21, 5),
    (8, 16, 18),
    (9, 16, 18), (9, 17, 120),
    (10, 13, 150), (10, 14, 120), (10, 6, 20),
    (11, 15, 250),
    (12, 4, 120), (12, 5, 100), (12, 6, 30), (12, 14, 120),
    (13, 17, 150), (13, 18, 2), (13, 19, 40),
    (14, 20, 80), (14, 18, 1), (14, 7, 40);

-- Más mesas y cuentas del evento 17
INSERT INTO mesa (numero_evento, numero_mesa, id_mesero) VALUES
    (17, 4, 2),
    (17, 5, 1),
    (17, 6, 2);

INSERT INTO cuenta (numero_evento, secuencial, numero_mesa, nombre) VALUES
    (17, 4, 2, 'Tíos'),
    (17, 5, 4, 'Familia Herrera'),
    (17, 6, 5, 'Compañeros de trabajo');

-- Las comandas de ejemplo pasan a esta noche: misma ronda, misma hora
UPDATE comanda SET fecha_hora = now() - make_interval(mins => CASE id_comanda
    WHEN 1 THEN 95 WHEN 2 THEN 80 WHEN 3 THEN 60 ELSE 50 END);
UPDATE pago SET fecha_hora = now() - interval '20 minutes';

-- Rondas nuevas. Cada INSERT es una ronda: now() vale lo mismo en toda la
-- transacción, así que sus comandas comparten la hora. Bebidas las prepara
-- Iván (bar, 4) y lo demás Rosa (cocina, 3); las que ya pasaron de pendiente
-- descuentan su receta del inventario al insertarse (tg_comanda_stock).
INSERT INTO comanda (fecha_hora, estado, numero_evento, secuencial, id_mesero, id_producto, id_bar_cocina, detalles) VALUES
    (now() - interval '40 minutes', 'lista',          17, 4, 1, 6,  4,    NULL),
    (now() - interval '40 minutes', 'lista',          17, 4, 1, 6,  4,    NULL),
    (now() - interval '40 minutes', 'en_preparacion', 17, 4, 1, 10, 3,    'aderezo aparte');

INSERT INTO comanda (fecha_hora, estado, numero_evento, secuencial, id_mesero, id_producto, id_bar_cocina, detalles) VALUES
    (now() - interval '25 minutes', 'pendiente',      17, 5, 2, 11, NULL, 'término medio'),
    (now() - interval '25 minutes', 'pendiente',      17, 5, 2, 7,  NULL, 'sin sal en el borde'),
    (now() - interval '25 minutes', 'pendiente',      17, 5, 2, 7,  NULL, 'sin sal en el borde'),
    (now() - interval '25 minutes', 'entregada',      17, 5, 2, 5,  4,    NULL);

INSERT INTO comanda (fecha_hora, estado, numero_evento, secuencial, id_mesero, id_producto, id_bar_cocina, detalles) VALUES
    (now() - interval '10 minutes', 'pendiente',      17, 6, 1, 9,  NULL, NULL),
    (now() - interval '10 minutes', 'pendiente',      17, 6, 1, 13, NULL, NULL),
    (now() - interval '10 minutes', 'pendiente',      17, 6, 1, 14, NULL, NULL);

INSERT INTO comanda (fecha_hora, estado, numero_evento, secuencial, id_mesero, id_producto, id_bar_cocina, detalles) VALUES
    (now() - interval '5 minutes',  'pendiente',      17, 2, 1, 8,  NULL, 'para compartir');

SELECT setval(pg_get_serial_sequence('usuario', 'id_usuario'), (SELECT max(id_usuario) FROM usuario));
SELECT setval(pg_get_serial_sequence('producto', 'id_producto'), (SELECT max(id_producto) FROM producto));
SELECT setval(pg_get_serial_sequence('ingrediente', 'id_ingrediente'), (SELECT max(id_ingrediente) FROM ingrediente));

COMMIT;
