import unreal

CARPETAS = ("Deco_Collision", "Deco_Colision")

subsistema = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actores = subsistema.get_all_level_actors()

cambiados = []
sin_componente = []

def en_carpeta(ruta):
    for carpeta in CARPETAS:
        if ruta == carpeta or ruta.startswith(carpeta + "/"):
            return True
    return False

with unreal.ScopedEditorTransaction("Activar colision en Deco_Collision"):
    for actor in actores:
        if not en_carpeta(str(actor.get_folder_path())):
            continue

        actor.set_actor_enable_collision(True)
        componentes = actor.get_components_by_class(unreal.PrimitiveComponent)
        if not componentes:
            sin_componente.append(actor.get_actor_label())
            continue

        for componente in componentes:
            componente.set_collision_profile_name("BlockAll")
            componente.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)

        cambiados.append(actor.get_actor_label())

unreal.log_warning("Colision activada en %d actores" % len(cambiados))
for nombre in cambiados:
    unreal.log("  " + nombre)
if sin_componente:
    unreal.log_warning("Sin componente de colision: " + ", ".join(sin_componente))
