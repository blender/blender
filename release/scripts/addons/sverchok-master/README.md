# English

## Sverchok parametric tools

**addon for**: [Blender](http://blender.org)  (version *2.78* and above).  
**current sverchok version**: Find version in addon settings or in the node Sverchok panel   
**License**: [GPL3](http://www.gnu.org/licenses/quick-guide-gplv3.html)   
**prerequisites**: Python 3.6, and `numpy` and `requests`, both included in recent versions of Blender (precompiled binaries are convenient for this)  
**manual**: [In English](http://nikitron.cc.ua/sverch/html/main.html) - This is an introduction to Sverchok and contains 3 lessons, and documentation on almost all nodes. If anything isn't clear (or missing) in this document please ask about it on the [Issue Tracker](https://github.com/nortikin/sverchok/issues), we want to get these lessons right and you can help us! 

  
### Description
Sverchok is a powerful parametric tool for architects, allowing geometry to be programmed visually with nodes. 
Mesh and geometry programming consists of combining basic elements such as:  

  - lists of indexed Vectors representing coordinates (Sverchok vectors are zero based)
  - lists of grouped indices to represent edges and polygons.
  - matrices (user-friendly rotation-scale-location transformations)

### Possibilities
Comes with more than 150 nodes to help create and manipulate geometry. Combining these nodes will allow you to:

  - do parametric constructions
  - easily change parameters with sliders and formulas    
  - power nodes such as: Profile parametric, UVconnect, Generative art, Mesh expression, Proportion edit, Wafel, Adaptive Poligons (tissue vectorized), Adaptive edges, ExecNodeMod, Vector Interpolation series of nodes, List manipulators, CSG Boolean, Bmesh ops, Bmesh props, etc.
  - do cross sections, extrusions, other modifications with hight level flexible parametrised and vectorised node tools 
  - calculate areas, volume, and perform other geometric analysis
  - make or import CSV tables or custom formats
  - use Vector fields, create them, visualize data
  - even code your own custom nodes in python with Scripted node
  - make your own 'addons' on node layouts and utilise them with Sverchok 3dview panel in your everyday pipeline
  - access to Blender Python API (bpy) with special _Set_ and _Get_ nodes
  - upgrade Sverchok with pressing one button
  - make your own neuro network
  - and much, much more!

### Installation
Install Sverchok as you would any blender addon.  
  
-  _Installation from Preferences_  
   Download Sverchok [archive (zip) from github](https://github.com/nortikin/sverchok/archive/master.zip)   
   User Preferences > Addons > install from file >  choose zip-archive > activate flag beside Sverchok  
   Enable permanently in the startup.blend using `Ctrl + U` and `Save User Settings` from the Addons menu.  

-  _Upgrade Sverchok on fly_   
   Use button `Check for new version` in sverchok panel in node editor (press `N` for panel).    
   Press `Update Sverchok` button.   
   At the end press F8 to reload add-ons. In NodeView the new version number will appear in the N-panel.   

### Troubleshooting Installation Errors

If you are installing from a release zip, please be aware that if it contains a folder named `sverchok-master.x.y.z`, you will need to rename that folder to `sverchok-master` because folder names with dots are not valid python package names. But it's best to just name it `sverchok`.  

If you are installing from a release found [here](https://github.com/nortikin/sverchok/releases), these files contain folders that have the dots mentioned in the previous point. These versioned release zips are not meant for installing from, but rather can be used to try older versions of Sverchok when you are using older .blend files and older Blender versions. Don't use these release zips if you are installing sverchok for the first time.

During install from preferences, if an error is raised - close and run Blender again and activate sverchok.  

In case Sverchok still fails to install, we've compiled a list of reasons and known resolutions [here](http://nikitron.cc.ua/sverch/html/installation.html). Please let us know if you encounter other installation issues.   

If you update with update button in sverchok panel it can raise an error if you renamed a folder, so follow [this](https://github.com/nortikin/sverchok/issues/669) (a bootstrap script you can run from TextEditor)  

### Contact and Credit
Homepage: [Home](http://nikitron.cc.ua/sverchok_en.html)  
Authors: 
-  Alexander Nedovizin,  
-  Nikita Gorodetskiy,  
-  Linus Yng,  
-  Agustin Gimenez, 
-  Dealga McArdle,  
-  Konstantin Vorobiew, 
-  Ilya Protnov,  
-  Eleanor Howick,    
-  Walter Perdan,    
-  Marius Giurgi,      
-  Durman,     
-  Ivan Prytov     

Email: sverchok-b3d@yandex.ru  

[![Please donate](https://www.paypalobjects.com/en_US/GB/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=JZESR6GN9AKNS)

# По-русски

**дополнение к**: [Blender](http://blender.org)  (версия *2.77* и выше).  
**текущая версия**: Смотри настройки Сверчка или панель   
**Лицензия**: [GPL3](http://www.gnu.org/licenses/quick-guide-gplv3.html)   
**требования**: Python 3.5, numpy, они оба присутствуют в Blender  


  
### Описание
Сверчок - мощный инструмент для архитектора, позволяющий визуально программировать узлами. 
Программирование сетки и геометрии состоит из "кирпичей":  

  - списков Векторов являющих собой координаты вершин  
  - списки групп индексов представляющие рёбра и грани  
  - матрицы (удобный и понятный способ изменения положения-масштаба-поворота)  

### Возможности
Более 150 узлов вам помогут создать и изменять геометрию. А сочетания узлов помогут вам:

  - делать параметрические конструкции
  - легко менять параметры слайдерами и формулами
  - Супер-узлы: Profile parametric, UVconnect, Generetive art, Mesh expression, Proportion edit, Wafel, Adaptive Poligons (tissue vectorized), Adaptive edges, ExecNodeMod, Vector Interpolation series of nodes, List manipulators, CSG Boolean, Bmesh ops,Bmesh props, и т.д.    
  - делать сечения, выдавливания, другие изменения с гибким параметризованым и векторизованым набором узловых инструментов  
  - считать площади, объём и прочее
  - анализировать геометрию
  - выводить данные в таблицы CSV или импортировать из CSV прямо в Сверчка
  - создавать векторные поля
  - визуализировать данные
  - даже написать свой узел на питоне, используя Scripted node
  - делать свои дополнения к блендеру раскладкой узлов и затем пользоваться ими в окне 3М вида при помощи панели инструментов Сверчка для 3М окна
  - доступ к API (bpy) при помощи узлов _Set_ и _Get_ 
  - обновлять сверчка одной кнопкой
  - делать свою собственную нейронную сеть
  - и даже больше   


### Установка
Установите как обычный адон к блендеру.  
  
-  _Установка из пользовательских настроек_  
   Скачать Сверчка с github  
   User Preferences > Addons > install from file >   выбрать zip-архив > активировать Сверчка  
   Подтвердите выбор в файле startup.blend используя `Ctrl + U` и `Save User Settings`в меню Addons.  

-  _Обновление Сверчка_   
   Используйте кнопку `Check for new version` в панели Сверчка в раскладке узлов (`N` чтобы вызвать). 
   Нажмите кнопку `Update Sverchok` там же.  
   Нажмите потом `F8` чтобы перезагрузить дополнения блендера. Должна поменяться версия.  

### Известные ошибки установки
Не установилось? Список причин [тыц](http://nikitron.cc.ua/sverch/html/installation.html). Если вашей ошибки там нет - пишите письма.  
Если вы устанавливаете из архива типа release, опасайтесь имени папки типа `sverchok-master.x.y.z`, в таком случае переименуйте её в `sverchok-master`, потому что имена папок с точками не читаются в именах пакетов питона.   
При установке из пользовательских настроек, при получении ошибки - закрыть блендер и снова активировать Сверчка, всё до сохранения настроек блендера.  
Также если вы обновляете Сверчка с кнопкой автообновления, будьте осторожны, в связи с названием папки, блендер может не подхватить её, следуйте указаниям [здесь](https://github.com/nortikin/sverchok/issues/669)   

### Контакты и разработчики
Домашняя страница: [Домой](http://nikitron.cc.ua/sverchok_ru.html)  
Разработчики: 
-  Недовизин Александр;  
-  Городецкий Никита;  
-  Инг Линус;  
-  Жименез Агустин; 
-  МакАрдле Деальга;  
-  Воробьёв Константин;  
-  Портнов Илья;  
-  Ховик Элеонора;  
-  Вальтер Пердан;    
-  Мариус Георгий     
-  Дурман,       
-  Портнов Иван      

Email: sverchok-b3d@yandex.ru  


[![Please donate](https://www.paypalobjects.com/en_US/GB/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=JZESR6GN9AKNS)
