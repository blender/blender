from scenegraph import Prototype, NULL, sceneGraph, IS, Script, ExternalPrototype, ROUTE
PROTO = Prototype
EXTERNPROTO = ExternalPrototype

Anchor = Prototype( "Anchor",
	{
		'bboxSize':('bboxSize', 'SFVec3f', 0),
		'children':('children', 'MFNode', 1),
		'parameter':('parameter', 'MFString', 1),
		'url':('url', 'MFString', 1),
		'description':('description', 'SFString', 1),
		'bboxCenter':('bboxCenter', 'SFVec3f', 0),
	},
	{
		'bboxSize':[-1.0, -1.0, -1.0],
		'children':[],
		'parameter':[],
		'url':[],
		'description':'',
		'bboxCenter':[0.0, 0.0, 0.0],
	},
	{
		'addChildren':('addChildren', 'MFNode', 0),
		'removeChildren':('removeChildren', 'MFNode', 0),
	},
)
Appearance = Prototype( "Appearance",
	{
		'material':('material', 'SFNode', 1),
		'texture':('texture', 'SFNode', 1),
		'textureTransform':('textureTransform', 'SFNode', 1),
	},
	{
		'material':NULL,
		'texture':NULL,
		'textureTransform':NULL,
	},
	{
	},
)
AudioClip = Prototype( "AudioClip",
	{
		'pitch':('pitch', 'SFFloat', 1),
		'loop':('loop', 'SFBool', 1),
		'description':('description', 'SFString', 1),
		'stopTime':('stopTime', 'SFTime', 1),
		'startTime':('startTime', 'SFTime', 1),
		'url':('url', 'MFString', 1),
	},
	{
		'pitch':1.0,
		'loop':0,
		'description':'',
		'stopTime':0.0,
		'startTime':0.0,
		'url':[],
	},
	{
		'isActive':('isActive', 'SFBool', 1),
		'duration_changed':('duration_changed', 'SFTime', 1),
	},
)
Background = Prototype( "Background",
	{
		'groundAngle':('groundAngle', 'MFFloat', 1),
		'skyAngle':('skyAngle', 'MFFloat', 1),
		'frontUrl':('frontUrl', 'MFString', 1),
		'bottomUrl':('bottomUrl', 'MFString', 1),
		'groundColor':('groundColor', 'MFColor', 1),
		'backUrl':('backUrl', 'MFString', 1),
		'skyColor':('skyColor', 'MFColor', 1),
		'topUrl':('topUrl', 'MFString', 1),
		'rightUrl':('rightUrl', 'MFString', 1),
		'leftUrl':('leftUrl', 'MFString', 1),
	},
	{
		'groundAngle':[],
		'skyAngle':[],
		'frontUrl':[],
		'bottomUrl':[],
		'groundColor':[],
		'backUrl':[],
		'skyColor':[[0.0, 0.0, 0.0]],
		'topUrl':[],
		'rightUrl':[],
		'leftUrl':[],
	},
	{
		'isBound':('isBound', 'SFBool', 1),
		'set_bind':('set_bind', 'SFBool', 0),
	},
)
Billboard = Prototype( "Billboard",
	{
		'bboxCenter':('bboxCenter', 'SFVec3f', 0),
		'bboxSize':('bboxSize', 'SFVec3f', 0),
		'children':('children', 'MFNode', 1),
		'axisOfRotation':('axisOfRotation', 'SFVec3f', 1),
	},
	{
		'bboxCenter':[0.0, 0.0, 0.0],
		'bboxSize':[-1.0, -1.0, -1.0],
		'children':[],
		'axisOfRotation':[0.0, 1.0, 0.0],
	},
	{
		'addChildren':('addChildren', 'MFNode', 0),
		'removeChildren':('removeChildren', 'MFNode', 0),
	},
)
Box = Prototype( "Box",
	{
		'size':('size', 'SFVec3f', 0),
	},
	{
		'size':[2.0, 2.0, 2.0],
	},
	{
	},
)


Collision = Prototype( "Collision",
	{
		'bboxCenter':('bboxCenter', 'SFVec3f', 0),
		'bboxSize':('bboxSize', 'SFVec3f', 0),
		'children':('children', 'MFNode', 1),
		'collide':('collide', 'SFBool', 1),
		'proxy':('proxy', 'SFNode', 0),
	},
	{
		'bboxCenter':[0.0, 0.0, 0.0],
		'bboxSize':[-1.0, -1.0, -1.0],
		'children':[],
		'collide':1,
		'proxy':NULL,
	},
	{
		'addChildren':('addChildren', 'MFNode', 0),
		'removeChildren':('removeChildren', 'MFNode', 0),
		'collideTime':('collideTime', 'SFTime', 1),
	},
)
Color = Prototype( "Color",
	{
		'color':('color', 'MFColor', 1),
	},
	{
		'color':[],
	},
	{
	},
)
ColorInterpolator = Prototype( "ColorInterpolator",
	{
		'key':('key', 'MFFloat', 1),
		'keyValue':('keyValue', 'MFColor', 1),
	},
	{
		'key':[],
		'keyValue':[],
	},
	{
		'value_changed':('value_changed', 'SFColor', 1),
		'set_fraction':('set_fraction', 'SFFloat', 0),
	},
)
Cone = Prototype( "Cone",
	{
		'bottomRadius':('bottomRadius', 'SFFloat', 0),
		'side':('side', 'SFBool', 0),
		'bottom':('bottom', 'SFBool', 0),
		'height':('height', 'SFFloat', 0),
	},
	{
		'bottomRadius':1.0,
		'side':1,
		'bottom':1,
		'height':2.0,
	},
	{
	},
)
Coordinate = Prototype( "Coordinate",
	{
		'point':('point', 'MFVec3f', 1),
	},
	{
		'point':[],
	},
	{
	},
)
CoordinateInterpolator = Prototype( "CoordinateInterpolator",
	{
		'key':('key', 'MFFloat', 1),
		'keyValue':('keyValue', 'MFVec3f', 1),
	},
	{
		'key':[],
		'keyValue':[],
	},
	{
		'value_changed':('value_changed', 'MFVec3f', 1),
		'set_fraction':('set_fraction', 'SFFloat', 0),
	},
)
Cylinder = Prototype( "Cylinder",
	{
		'bottom':('bottom', 'SFBool', 0),
		'side':('side', 'SFBool', 0),
		'radius':('radius', 'SFFloat', 0),
		'top':('top', 'SFBool', 0),
		'height':('height', 'SFFloat', 0),
	},
	{
		'bottom':1,
		'side':1,
		'radius':1.0,
		'top':1,
		'height':2.0,
	},
	{
	},
)
CylinderSensor = Prototype( "CylinderSensor",
	{
		'maxAngle':('maxAngle', 'SFFloat', 1),
		'autoOffset':('autoOffset', 'SFBool', 1),
		'minAngle':('minAngle', 'SFFloat', 1),
		'enabled':('enabled', 'SFBool', 1),
		'offset':('offset', 'SFFloat', 1),
		'diskAngle':('diskAngle', 'SFFloat', 1),
	},
	{
		'maxAngle':-1.0,
		'autoOffset':1,
		'minAngle':0.0,
		'enabled':1,
		'offset':0.0,
		'diskAngle':0.262,
	},
	{
		'rotation_changed':('rotation_changed', 'SFRotation', 1),
		'isActive':('isActive', 'SFBool', 1),
		'trackPoint_changed':('trackPoint_changed', 'SFVec3f', 1),
	},
)
DirectionalLight = Prototype( "DirectionalLight",
	{
		'color':('color', 'SFColor', 1),
		'ambientIntensity':('ambientIntensity', 'SFFloat', 1),
		'intensity':('intensity', 'SFFloat', 1),
		'on':('on', 'SFBool', 1),
		'direction':('direction', 'SFVec3f', 1),
	},
	{
		'color':[1.0, 1.0, 1.0],
		'ambientIntensity':0.0,
		'intensity':1.0,
		'on':1,
		'direction':[0.0, 0.0, -1.0],
	},
	{
	},
)
ElevationGrid = Prototype( "ElevationGrid",
	{
		'xSpacing':('xSpacing', 'SFFloat', 0),
		'zSpacing':('zSpacing', 'SFFloat', 0),
		'xDimension':('xDimension', 'SFInt32', 0),
		'colorPerVertex':('colorPerVertex', 'SFBool', 0),
		'height':('height', 'MFFloat', 0),
		'texCoord':('texCoord', 'SFNode', 1),
		'normalPerVertex':('normalPerVertex', 'SFBool', 0),
		'ccw':('ccw', 'SFBool', 0),
		'color':('color', 'SFNode', 1),
		'normal':('normal', 'SFNode', 1),
		'creaseAngle':('creaseAngle', 'SFFloat', 0),
		'solid':('solid', 'SFBool', 0),
		'zDimension':('zDimension', 'SFInt32', 0),
	},
	{
		'xSpacing':0.0,
		'zSpacing':0.0,
		'xDimension':0,
		'colorPerVertex':1,
		'height':[],
		'texCoord':NULL,
		'normalPerVertex':1,
		'ccw':1,
		'color':NULL,
		'normal':NULL,
		'creaseAngle':0.0,
		'solid':1,
		'zDimension':0,
	},
	{
		'set_height':('set_height', 'MFFloat', 0),
	},
)
Extrusion = Prototype( "Extrusion",
	{
		'endCap':('endCap', 'SFBool', 0),
		'scale':('scale', 'MFVec2f', 0),
		'ccw':('ccw', 'SFBool', 0),
		'crossSection':('crossSection', 'MFVec2f', 0),
		'solid':('solid', 'SFBool', 0),
		'convex':('convex', 'SFBool', 0),
		'creaseAngle':('creaseAngle', 'SFFloat', 0),
		'spine':('spine', 'MFVec3f', 0),
		'beginCap':('beginCap', 'SFBool', 0),
		'orientation':('orientation', 'MFRotation', 0),
	},
	{
		'endCap':1,
		'scale':[[1.0, 1.0]],
		'ccw':1,
		'crossSection':[[1.0, 1.0], [1.0, -1.0], [-1.0, -1.0], [-1.0, 1.0], [1.0, 1.0]],
		'solid':1,
		'convex':1,
		'creaseAngle':0.0,
		'spine':[[0.0, 0.0, 0.0], [0.0, 1.0, 0.0]],
		'beginCap':1,
		'orientation':[[0.0, 0.0, 1.0, 0.0]],
	},
	{
		'set_scale':('set_scale', 'MFVec2f', 0),
		'set_spine':('set_spine', 'MFVec3f', 0),
		'set_orientation':('set_orientation', 'MFRotation', 0),
		'set_crossSection':('set_crossSection', 'MFVec2f', 0),
	},
)
Fog = Prototype( "Fog",
	{
		'fogType':('fogType', 'SFString', 1),
		'color':('color', 'SFColor', 1),
		'visibilityRange':('visibilityRange', 'SFFloat', 1),
	},
	{
		'fogType':'LINEAR',
		'color':[1.0, 1.0, 1.0],
		'visibilityRange':0.0,
	},
	{
		'isBound':('isBound', 'SFBool', 1),
		'set_bind':('set_bind', 'SFBool', 0),
	},
)
FontStyle = Prototype( "FontStyle",
	{
		'justify':('justify', 'MFString', 0),
		'leftToRight':('leftToRight', 'SFBool', 0),
		'spacing':('spacing', 'SFFloat', 0),
		'horizontal':('horizontal', 'SFBool', 0),
		'language':('language', 'SFString', 0),
		'topToBottom':('topToBottom', 'SFBool', 0),
		'size':('size', 'SFFloat', 0),
		'style':('style', 'SFString', 0),
		'family':('family', 'SFString', 0),
	},
	{
		'justify':['BEGIN'],
		'leftToRight':1,
		'spacing':1.0,
		'horizontal':1,
		'language':'',
		'topToBottom':1,
		'size':1.0,
		'style':'PLAIN',
		'family':'SERIF',
	},
	{
	},
)
Group = Prototype( "Group",
	{
		'bboxSize':('bboxSize', 'SFVec3f', 0),
		'children':('children', 'MFNode', 1),
		'bboxCenter':('bboxCenter', 'SFVec3f', 0),
	},
	{
		'bboxSize':[-1.0, -1.0, -1.0],
		'children':[],
		'bboxCenter':[0.0, 0.0, 0.0],
	},
	{
		'addChildren':('addChildren', 'MFNode', 0),
		'removeChildren':('removeChildren', 'MFNode', 0),
	},
)
ImageTexture = Prototype( "ImageTexture",
	{
		'repeatS':('repeatS', 'SFBool', 0),
		'url':('url', 'MFString', 1),
		'repeatT':('repeatT', 'SFBool', 0),
	},
	{
		'repeatS':1,
		'url':[],
		'repeatT':1,
	},
	{
	},
)
IndexedFaceSet = Prototype( "IndexedFaceSet",
	{
		'texCoordIndex':('texCoordIndex', 'MFInt32', 0),
		'normalIndex':('normalIndex', 'MFInt32', 0),
		'coordIndex':('coordIndex', 'MFInt32', 0),
		'convex':('convex', 'SFBool', 0),
		'texCoord':('texCoord', 'SFNode', 1),
		'normalPerVertex':('normalPerVertex', 'SFBool', 0),
		'coord':('coord', 'SFNode', 1),
		'ccw':('ccw', 'SFBool', 0),
		'color':('color', 'SFNode', 1),
		'normal':('normal', 'SFNode', 1),
		'creaseAngle':('creaseAngle', 'SFFloat', 0),
		'solid':('solid', 'SFBool', 0),
		'colorPerVertex':('colorPerVertex', 'SFBool', 0),
		'colorIndex':('colorIndex', 'MFInt32', 0),
	},
	{
		'texCoordIndex':[],
		'normalIndex':[],
		'coordIndex':[],
		'convex':1,
		'texCoord':NULL,
		'normalPerVertex':1,
		'coord':NULL,
		'ccw':1,
		'color':NULL,
		'normal':NULL,
		'creaseAngle':0.0,
		'solid':1,
		'colorPerVertex':1,
		'colorIndex':[],
	},
	{
		'set_normalIndex':('set_normalIndex', 'MFInt32', 0),
		'set_colorIndex':('set_colorIndex', 'MFInt32', 0),
		'set_texCoordIndex':('set_texCoordIndex', 'MFInt32', 0),
		'set_coordIndex':('set_coordIndex', 'MFInt32', 0),
	},
)
IndexedLineSet = Prototype( "IndexedLineSet",
	{
		'coordIndex':('coordIndex', 'MFInt32', 0),
		'coord':('coord', 'SFNode', 1),
		'colorIndex':('colorIndex', 'MFInt32', 0),
		'colorPerVertex':('colorPerVertex', 'SFBool', 0),
		'color':('color', 'SFNode', 1),
	},
	{
		'coordIndex':[],
		'coord':NULL,
		'colorIndex':[],
		'colorPerVertex':1,
		'color':NULL,
	},
	{
		'set_colorIndex':('set_colorIndex', 'MFInt32', 0),
		'set_coordIndex':('set_coordIndex', 'MFInt32', 0),
	},
)
Inline = Prototype( "Inline",
	{
		'url':('url', 'MFString', 1),
		'bboxSize':('bboxSize', 'SFVec3f', 0),
		'bboxCenter':('bboxCenter', 'SFVec3f', 0),
	},
	{
		'url':[],
		'bboxSize':[-1.0, -1.0, -1.0],
		'bboxCenter':[0.0, 0.0, 0.0],
	},
	{
	},
)
LOD = Prototype( "LOD",
	{
		'level':('level', 'MFNode', 1),
		'range':('range', 'MFFloat', 0),
		'center':('center', 'SFVec3f', 0),
	},
	{
		'level':[],
		'range':[],
		'center':[0.0, 0.0, 0.0],
	},
	{
	},
)
Material = Prototype( "Material",
	{
		'emissiveColor':('emissiveColor', 'SFColor', 1),
		'transparency':('transparency', 'SFFloat', 1),
		'shininess':('shininess', 'SFFloat', 1),
		'diffuseColor':('diffuseColor', 'SFColor', 1),
		'ambientIntensity':('ambientIntensity', 'SFFloat', 1),
		'specularColor':('specularColor', 'SFColor', 1),
	},
	{
		'emissiveColor':[0.0, 0.0, 0.0],
		'transparency':0.0,
		'shininess':0.2,
		'diffuseColor':[0.8, 0.8, 0.8],
		'ambientIntensity':0.2,
		'specularColor':[0.0, 0.0, 0.0],
	},
	{
	},
)
MovieTexture = Prototype( "MovieTexture",
	{
		'loop':('loop', 'SFBool', 1),
		'speed':('speed', 'SFFloat', 1),
		'repeatT':('repeatT', 'SFBool', 0),
		'repeatS':('repeatS', 'SFBool', 0),
		'url':('url', 'MFString', 1),
		'startTime':('startTime', 'SFTime', 1),
		'stopTime':('stopTime', 'SFTime', 1),
	},
	{
		'loop':0,
		'speed':1.0,
		'repeatT':1,
		'repeatS':1,
		'url':[],
		'startTime':0.0,
		'stopTime':0.0,
	},
	{
		'isActive':('isActive', 'SFBool', 1),
		'duration_changed':('duration_changed', 'SFFloat', 1),
	},
)
NavigationInfo = Prototype( "NavigationInfo",
	{
		'avatarSize':('avatarSize', 'MFFloat', 1),
		'speed':('speed', 'SFFloat', 1),
		'headlight':('headlight', 'SFBool', 1),
		'visibilityLimit':('visibilityLimit', 'SFFloat', 1),
		'type':('type', 'MFString', 1),
	},
	{
		'avatarSize':[0.25, 1.6, 0.75],
		'speed':1.0,
		'headlight':1,
		'visibilityLimit':0.0,
		'type':['WALK'],
	},
	{
		'isBound':('isBound', 'SFBool', 1),
		'set_bind':('set_bind', 'SFBool', 0),
	},
)
Normal = Prototype( "Normal",
	{
		'vector':('vector', 'MFVec3f', 1),
	},
	{
		'vector':[],
	},
	{
	},
)
NormalInterpolator = Prototype( "NormalInterpolator",
	{
		'key':('key', 'MFFloat', 1),
		'keyValue':('keyValue', 'MFVec3f', 1),
	},
	{
		'key':[],
		'keyValue':[],
	},
	{
		'value_changed':('value_changed', 'MFVec3f', 1),
		'set_fraction':('set_fraction', 'SFFloat', 0),
	},
)
OrientationInterpolator = Prototype( "OrientationInterpolator",
	{
		'key':('key', 'MFFloat', 1),
		'keyValue':('keyValue', 'MFRotation', 1),
	},
	{
		'key':[],
		'keyValue':[],
	},
	{
		'value_changed':('value_changed', 'SFRotation', 1),
		'set_fraction':('set_fraction', 'SFFloat', 0),
	},
)
PixelTexture = Prototype( "PixelTexture",
	{
		'repeatS':('repeatS', 'SFBool', 0),
		'image':('image', 'SFImage', 1),
		'repeatT':('repeatT', 'SFBool', 0),
	},
	{
		'repeatS':1,
		'image':[0, 0, 0],
		'repeatT':1,
	},
	{
	},
)
PlaneSensor = Prototype( "PlaneSensor",
	{
		'offset':('offset', 'SFVec3f', 1),
		'autoOffset':('autoOffset', 'SFBool', 1),
		'minPosition':('minPosition', 'SFVec2f', 1),
		'enabled':('enabled', 'SFBool', 1),
		'maxPosition':('maxPosition', 'SFVec2f', 1),
	},
	{
		'offset':[0.0, 0.0, 0.0],
		'autoOffset':1,
		'minPosition':[0.0, 0.0],
		'enabled':1,
		'maxPosition':[-1.0, -1.0],
	},
	{
		'translation_changed':('translation_changed', 'SFVec3f', 1),
		'isActive':('isActive', 'SFBool', 1),
		'trackPoint_changed':('trackPoint_changed', 'SFVec3f', 1),
	},
)
PointLight = Prototype( "PointLight",
	{
		'ambientIntensity':('ambientIntensity', 'SFFloat', 1),
		'color':('color', 'SFColor', 1),
		'location':('location', 'SFVec3f', 1),
		'radius':('radius', 'SFFloat', 1),
		'attenuation':('attenuation', 'SFVec3f', 1),
		'intensity':('intensity', 'SFFloat', 1),
		'on':('on', 'SFBool', 1),
	},
	{
		'ambientIntensity':0.0,
		'color':[1.0, 1.0, 1.0],
		'location':[0.0, 0.0, 0.0],
		'radius':100.0,
		'attenuation':[1.0, 0.0, 0.0],
		'intensity':1.0,
		'on':1,
	},
	{
	},
)
PointSet = Prototype( "PointSet",
	{
		'coord':('coord', 'SFNode', 1),
		'color':('color', 'SFNode', 1),
	},
	{
		'coord':NULL,
		'color':NULL,
	},
	{
	},
)
PositionInterpolator = Prototype( "PositionInterpolator",
	{
		'key':('key', 'MFFloat', 1),
		'keyValue':('keyValue', 'MFVec3f', 1),
	},
	{
		'key':[],
		'keyValue':[],
	},
	{
		'value_changed':('value_changed', 'SFVec3f', 1),
		'set_fraction':('set_fraction', 'SFFloat', 0),
	},
)
ProximitySensor = Prototype( "ProximitySensor",
	{
		'size':('size', 'SFVec3f', 1),
		'center':('center', 'SFVec3f', 1),
		'enabled':('enabled', 'SFBool', 1),
	},
	{
		'size':[0.0, 0.0, 0.0],
		'center':[0.0, 0.0, 0.0],
		'enabled':1,
	},
	{
		'enterTime':('enterTime', 'SFTime', 1),
		'isActive':('isActive', 'SFBool', 1),
		'orientation_changed':('orientation_changed', 'SFRotation', 1),
		'exitTime':('exitTime', 'SFTime', 1),
		'position_changed':('position_changed', 'SFVec3f', 1),
	},
)
ScalarInterpolator = Prototype( "ScalarInterpolator",
	{
		'key':('key', 'MFFloat', 1),
		'keyValue':('keyValue', 'MFFloat', 1),
	},
	{
		'key':[],
		'keyValue':[],
	},
	{
		'value_changed':('value_changed', 'SFFloat', 1),
		'set_fraction':('set_fraction', 'SFFloat', 0),
	},
)
Shape = Prototype( "Shape",
	{
		'appearance':('appearance', 'SFNode', 1),
		'geometry':('geometry', 'SFNode', 1),
	},
	{
		'appearance':NULL,
		'geometry':NULL,
	},
	{
	},
)
Sound = Prototype( "Sound",
	{
		'spatialize':('spatialize', 'SFBool', 0),
		'maxFront':('maxFront', 'SFFloat', 1),
		'minBack':('minBack', 'SFFloat', 1),
		'maxBack':('maxBack', 'SFFloat', 1),
		'minFront':('minFront', 'SFFloat', 1),
		'location':('location', 'SFVec3f', 1),
		'intensity':('intensity', 'SFFloat', 1),
		'direction':('direction', 'SFVec3f', 1),
		'source':('source', 'SFNode', 1),
		'priority':('priority', 'SFFloat', 1),
	},
	{
		'spatialize':1,
		'maxFront':10.0,
		'minBack':1.0,
		'maxBack':10.0,
		'minFront':1.0,
		'location':[0.0, 0.0, 0.0],
		'intensity':1.0,
		'direction':[0.0, 0.0, 1.0],
		'source':NULL,
		'priority':0.0,
	},
	{
	},
)
Sphere = Prototype( "Sphere",
	{
		'radius':('radius', 'SFFloat', 0),
	},
	{
		'radius':1.0,
	},
	{
	},
)
SphereSensor = Prototype( "SphereSensor",
	{
		'offset':('offset', 'SFRotation', 1),
		'autoOffset':('autoOffset', 'SFBool', 1),
		'enabled':('enabled', 'SFBool', 1),
	},
	{
		'offset':[0.0, 1.0, 0.0, 0.0],
		'autoOffset':1,
		'enabled':1,
	},
	{
		'rotation_changed':('rotation_changed', 'SFRotation', 1),
		'isActive':('isActive', 'SFBool', 1),
		'trackPoint_changed':('trackPoint_changed', 'SFVec3f', 1),
	},
)
SpotLight = Prototype( "SpotLight",
	{
		'attenuation':('attenuation', 'SFVec3f', 1),
		'ambientIntensity':('ambientIntensity', 'SFFloat', 1),
		'cutOffAngle':('cutOffAngle', 'SFFloat', 1),
		'direction':('direction', 'SFVec3f', 1),
		'color':('color', 'SFColor', 1),
		'location':('location', 'SFVec3f', 1),
		'radius':('radius', 'SFFloat', 1),
		'intensity':('intensity', 'SFFloat', 1),
		'beamWidth':('beamWidth', 'SFFloat', 1),
		'on':('on', 'SFBool', 1),
	},
	{
		'attenuation':[1.0, 0.0, 0.0],
		'ambientIntensity':0.0,
		'cutOffAngle':0.785398,
		'direction':[0.0, 0.0, -1.0],
		'color':[1.0, 1.0, 1.0],
		'location':[0.0, 0.0, 0.0],
		'radius':100.0,
		'intensity':1.0,
		'beamWidth':1.570796,
		'on':1,
	},
	{
	},
)
Switch = Prototype( "Switch",
	{
		'choice':('choice', 'MFNode', 1),
		'whichChoice':('whichChoice', 'SFInt32', 1),
	},
	{
		'choice':[],
		'whichChoice':-1,
	},
	{
	},
)
Text = Prototype( "Text",
	{
		'maxExtent':('maxExtent', 'SFFloat', 1),
		'string':('string', 'MFString', 1),
		'fontStyle':('fontStyle', 'SFNode', 1),
		'length':('length', 'MFFloat', 1),
	},
	{
		'maxExtent':0.0,
		'string':[],
		'fontStyle':NULL,
		'length':[],
	},
	{
	},
)
TextureCoordinate = Prototype( "TextureCoordinate",
	{
		'point':('point', 'MFVec2f', 1),
	},
	{
		'point':[],
	},
	{
	},
)
TextureTransform = Prototype( "TextureTransform",
	{
		'center':('center', 'SFVec2f', 1),
		'scale':('scale', 'SFVec2f', 1),
		'rotation':('rotation', 'SFFloat', 1),
		'translation':('translation', 'SFVec2f', 1),
	},
	{
		'center':[0.0, 0.0],
		'scale':[1.0, 1.0],
		'rotation':0.0,
		'translation':[0.0, 0.0],
	},
	{
	},
)
TimeSensor = Prototype( "TimeSensor",
	{
		'loop':('loop', 'SFBool', 1),
		'cycleInterval':('cycleInterval', 'SFTime', 1),
		'enabled':('enabled', 'SFBool', 1),
		'stopTime':('stopTime', 'SFTime', 1),
		'startTime':('startTime', 'SFTime', 1),
	},
	{
		'loop':0,
		'cycleInterval':1.0,
		'enabled':1,
		'stopTime':0.0,
		'startTime':0.0,
	},
	{
		'fraction_changed':('fraction_changed', 'SFFloat', 1),
		'isActive':('isActive', 'SFBool', 1),
		'time':('time', 'SFTime', 1),
		'cycleTime':('cycleTime', 'SFTime', 1),
	},
)
TouchSensor = Prototype( "TouchSensor",
	{
		'enabled':('enabled', 'SFBool', 1),
	},
	{
		'enabled':1,
	},
	{
		'hitNormal_changed':('hitNormal_changed', 'SFVec3f', 1),
		'hitPoint_changed':('hitPoint_changed', 'SFVec3f', 1),
		'touchTime':('touchTime', 'SFTime', 1),
		'hitTexCoord_changed':('hitTexCoord_changed', 'SFVec2f', 1),
		'isActive':('isActive', 'SFBool', 1),
		'isOver':('isOver', 'SFBool', 1),
	},
)
Transform = Prototype( "Transform",
	{
		'bboxSize':('bboxSize', 'SFVec3f', 0),
		'children':('children', 'MFNode', 1),
		'scaleOrientation':('scaleOrientation', 'SFRotation', 1),
		'rotation':('rotation', 'SFRotation', 1),
		'translation':('translation', 'SFVec3f', 1),
		'bboxCenter':('bboxCenter', 'SFVec3f', 0),
		'center':('center', 'SFVec3f', 1),
		'scale':('scale', 'SFVec3f', 1),
	},
	{
		'bboxSize':[-1.0, -1.0, -1.0],
		'children':[],
		'scaleOrientation':[0.0, 0.0, 1.0, 0.0],
		'rotation':[0.0, 0.0, 1.0, 0.0],
		'translation':[0.0, 0.0, 0.0],
		'bboxCenter':[0.0, 0.0, 0.0],
		'center':[0.0, 0.0, 0.0],
		'scale':[1.0, 1.0, 1.0],
	},
	{
		'addChildren':('addChildren', 'MFNode', 0),
		'removeChildren':('removeChildren', 'MFNode', 0),
	},
)
Viewpoint = Prototype( "Viewpoint",
	{
		'jump':('jump', 'SFBool', 1),
		'orientation':('orientation', 'SFRotation', 1),
		'fieldOfView':('fieldOfView', 'SFFloat', 1),
		'position':('position', 'SFVec3f', 1),
		'description':('description', 'SFString', 0),
	},
	{
		'jump':1,
		'orientation':[0.0, 0.0, 1.0, 0.0],
		'fieldOfView':0.785398,
		'position':[0.0, 0.0, 10.0],
		'description':'',
	},
	{
		'isBound':('isBound', 'SFBool', 1),
		'set_bind':('set_bind', 'SFBool', 0),
		'bindTime':('bindTime', 'SFTime', 1),
	},
)
VisibilitySensor = Prototype( "VisibilitySensor",
	{
		'size':('size', 'SFVec3f', 1),
		'center':('center', 'SFVec3f', 1),
		'enabled':('enabled', 'SFBool', 1),
	},
	{
		'size':[0.0, 0.0, 0.0],
		'center':[0.0, 0.0, 0.0],
		'enabled':1,
	},
	{
		'exitTime':('exitTime', 'SFTime', 1),
		'isActive':('isActive', 'SFBool', 1),
		'enterTime':('enterTime', 'SFTime', 1),
	},
)
WorldInfo = Prototype( "WorldInfo",
	{
		'title':('title', 'SFString', 0),
		'info':('info', 'MFString', 0),
	},
	{
		'title':'',
		'info':[],
	},
	{
	},
)
