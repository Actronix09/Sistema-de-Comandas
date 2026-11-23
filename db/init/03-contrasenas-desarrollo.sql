-- Solo para desarrollo: datos-ejemplo.sql trae hashes de muestra con los que nadie
-- puede entrar. Con esto, todos los usuarios de ejemplo entran con Demo_123.
-- (con punto al final, para cumplir validar_password: dos caracteres especiales).
-- En la PC de caja este archivo no se carga.
UPDATE usuario SET contrasena_hash = '$argon2id$v=19$m=65536,t=2,p=1$nGu+W6d6mrShpD4d9/xhCg$BvyPZGhNMWcwL7ArkOgVjGXQGDT2JOhMf726wTgPuAQ';
