import unreal

CARPETA = "Deco_Collision"

subsistema = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actores = subsistema.get_all_level_actors()

cambiados = []
sin_componente = []

with unreal.ScopedEditorTransaction("Activar colision en Deco_Collision"):
    for actor in actores:
        carpeta = str(actor.get_folder_path())
        if carpeta != CARPETA and not carpeta.startswith(CARPETA + "/"):
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

unreal.log_warning("Colision activada en %d actores de %s" % (len(cambiados), CARPETA))
for nombre in cambiados:
    unreal.log("  " + nombre)
if sin_componente:
    unreal.log_warning("Sin componente de colision: " + ", ".join(sin_componente))
