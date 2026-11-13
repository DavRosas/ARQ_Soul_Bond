import unreal

CARPETAS = ("Deco_NoCollision", "Playground", "Deco_Collision", "Deco_Colision")

subsistema = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actores = subsistema.get_all_level_actors()

cambiados = []
omitidos = []

def en_carpeta(ruta):
    for carpeta in CARPETAS:
        if ruta == carpeta or ruta.startswith(carpeta + "/"):
            return True
    return False

with unreal.ScopedEditorTransaction("Quietar mallas de decoracion"):
    for actor in actores:
        if not en_carpeta(str(actor.get_folder_path())):
            continue

        if not isinstance(actor, unreal.StaticMeshActor):
            omitidos.append(actor.get_actor_label())
            continue

        actor.set_actor_tick_enabled(False)
        malla = actor.static_mesh_component
        if malla:
            malla.set_mobility(unreal.ComponentMobility.STATIC)

        cambiados.append(actor.get_actor_label())

unreal.log_warning("Mallas quietas: %d" % len(cambiados))
for nombre in cambiados:
    unreal.log("  " + nombre)
if omitidos:
    unreal.log_warning("No se tocaron (no son malla estatica): " + ", ".join(omitidos))
