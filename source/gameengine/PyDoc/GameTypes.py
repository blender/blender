# $Id: GameLogic.py 19483 2009-03-31 21:03:15Z ben2610 $
"""
GameEngine Types
================
@var BL_ActionActuator: L{BL_ActionActuator<BL_ActionActuator.BL_ActionActuator>}
@var BL_Shader: L{BL_Shader<BL_Shader.BL_Shader>}
@var BL_ShapeActionActuator: L{BL_ShapeActionActuator<BL_ShapeActionActuator.BL_ShapeActionActuator>}
@var CListValue: L{CListValue<CListValue.CListValue>}
@var CValue: L{CValue<CValue.CValue>}
@var KX_BlenderMaterial: L{KX_BlenderMaterial<KX_BlenderMaterial.KX_BlenderMaterial>}
@var KX_CDActuator: L{KX_CDActuator<KX_CDActuator.KX_CDActuator>}
@var KX_Camera: L{KX_Camera<KX_Camera.KX_Camera>}
@var KX_CameraActuator: L{KX_CameraActuator<KX_CameraActuator.KX_CameraActuator>}
@var KX_ConstraintActuator: L{KX_ConstraintActuator<KX_ConstraintActuator.KX_ConstraintActuator>}
@var KX_ConstraintWrapper: L{KX_ConstraintWrapper<KX_ConstraintWrapper.KX_ConstraintWrapper>}
@var KX_GameActuator: L{KX_GameActuator<KX_GameActuator.KX_GameActuator>}
@var KX_GameObject: L{KX_GameObject<KX_GameObject.KX_GameObject>}
@var KX_IpoActuator: L{KX_IpoActuator<KX_IpoActuator.KX_IpoActuator>}
@var KX_LightObject: L{KX_LightObject<KX_LightObject.KX_LightObject>}
@var KX_MeshProxy: L{KX_MeshProxy<KX_MeshProxy.KX_MeshProxy>}
@var KX_MouseFocusSensor: L{KX_MouseFocusSensor<KX_MouseFocusSensor.KX_MouseFocusSensor>}
@var KX_NearSensor: L{KX_NearSensor<KX_NearSensor.KX_NearSensor>}
@var KX_NetworkMessageActuator: L{KX_NetworkMessageActuator<KX_NetworkMessageActuator.KX_NetworkMessageActuator>}
@var KX_NetworkMessageSensor: L{KX_NetworkMessageSensor<KX_NetworkMessageSensor.KX_NetworkMessageSensor>}
@var KX_ObjectActuator: L{KX_ObjectActuator<KX_ObjectActuator.KX_ObjectActuator>}
@var KX_ParentActuator: L{KX_ParentActuator<KX_ParentActuator.KX_ParentActuator>}
@var KX_PhysicsObjectWrapper: L{KX_PhysicsObjectWrapper<KX_PhysicsObjectWrapper.KX_PhysicsObjectWrapper>}
@var KX_PolyProxy: L{KX_PolyProxy<KX_PolyProxy.KX_PolyProxy>}
@var KX_PolygonMaterial: L{KX_PolygonMaterial<KX_PolygonMaterial.KX_PolygonMaterial>}
@var KX_RadarSensor: L{KX_RadarSensor<KX_RadarSensor.KX_RadarSensor>}
@var KX_RaySensor: L{KX_RaySensor<KX_RaySensor.KX_RaySensor>}
@var KX_SCA_AddObjectActuator: L{KX_SCA_AddObjectActuator<KX_SCA_AddObjectActuator.KX_SCA_AddObjectActuator>}
@var KX_SCA_DynamicActuator: L{KX_SCA_DynamicActuator<KX_SCA_DynamicActuator.KX_SCA_DynamicActuator>}
@var KX_SCA_EndObjectActuator: L{KX_SCA_EndObjectActuator<KX_SCA_EndObjectActuator.KX_SCA_EndObjectActuator>}
@var KX_SCA_ReplaceMeshActuator: L{KX_SCA_ReplaceMeshActuator<KX_SCA_ReplaceMeshActuator.KX_SCA_ReplaceMeshActuator>}
@var KX_Scene: L{KX_Scene<KX_Scene.KX_Scene>}
@var KX_SceneActuator: L{KX_SceneActuator<KX_SceneActuator.KX_SceneActuator>}
@var KX_SoundActuator: L{KX_SoundActuator<KX_SoundActuator.KX_SoundActuator>}
@var KX_StateActuator: L{KX_StateActuator<KX_StateActuator.KX_StateActuator>}
@var KX_TouchSensor: L{KX_TouchSensor<KX_TouchSensor.KX_TouchSensor>}
@var KX_TrackToActuator: L{KX_TrackToActuator<KX_TrackToActuator.KX_TrackToActuator>}
@var KX_VehicleWrapper: L{KX_VehicleWrapper<KX_VehicleWrapper.KX_VehicleWrapper>}
@var KX_VertexProxy: L{KX_VertexProxy<KX_VertexProxy.KX_VertexProxy>}
@var KX_VisibilityActuator: L{KX_VisibilityActuator<KX_VisibilityActuator.KX_VisibilityActuator>}
@var PyObjectPlus: L{PyObjectPlus<PyObjectPlus.PyObjectPlus>}
@var SCA_2DFilterActuator: L{SCA_2DFilterActuator<SCA_2DFilterActuator.SCA_2DFilterActuator>}
@var SCA_ANDController: L{SCA_ANDController<SCA_ANDController.SCA_ANDController>}
@var SCA_ActuatorSensor: L{SCA_ActuatorSensor<SCA_ActuatorSensor.SCA_ActuatorSensor>}
@var SCA_AlwaysSensor: L{SCA_AlwaysSensor<SCA_AlwaysSensor.SCA_AlwaysSensor>}
@var SCA_DelaySensor: L{SCA_DelaySensor<SCA_DelaySensor.SCA_DelaySensor>}
@var SCA_ILogicBrick: L{SCA_ILogicBrick<SCA_ILogicBrick.SCA_ILogicBrick>}
@var SCA_IObject: L{SCA_IObject<SCA_IObject.SCA_IObject>}
@var SCA_ISensor: L{SCA_ISensor<SCA_ISensor.SCA_ISensor>}
@var SCA_JoystickSensor: L{SCA_JoystickSensor<SCA_JoystickSensor.SCA_JoystickSensor>}
@var SCA_KeyboardSensor: L{SCA_KeyboardSensor<SCA_KeyboardSensor.SCA_KeyboardSensor>}
@var SCA_MouseSensor: L{SCA_MouseSensor<SCA_MouseSensor.SCA_MouseSensor>}
@var SCA_NANDController: L{SCA_NANDController<SCA_NANDController.SCA_NANDController>}
@var SCA_NORController: L{SCA_NORController<SCA_NORController.SCA_NORController>}
@var SCA_ORController: L{SCA_ORController<SCA_ORController.SCA_ORController>}
@var SCA_PropertyActuator: L{SCA_PropertyActuator<SCA_PropertyActuator.SCA_PropertyActuator>}
@var SCA_PropertySensor: L{SCA_PropertySensor<SCA_PropertySensor.SCA_PropertySensor>}
@var SCA_PythonController: L{SCA_PythonController<SCA_PythonController.SCA_PythonController>}
@var SCA_RandomActuator: L{SCA_RandomActuator<SCA_RandomActuator.SCA_RandomActuator>}
@var SCA_RandomSensor: L{SCA_RandomSensor<SCA_RandomSensor.SCA_RandomSensor>}
@var SCA_XNORController: L{SCA_XNORController<SCA_XNORController.SCA_XNORController>}
@var SCA_XORController: L{SCA_XORController<SCA_XORController.SCA_XORController>}
"""

if 0:
	# Use to print out all the links
	for i in a.split('\n'):
		if i.startswith('@var'):
			var = i.split(' ')[1].split(':')[0]
			print '@var %s: L{%s<%s.%s>}' % (var, var, var, var)

