#!/usr/bin/python

import os,sys,string

install_dependencies=["GLUT",\
                          "QtCore", "QtGui", "QtXml", \
                          "libQGLViewer", "FreestyleSystem", "FreestyleRendering",\
                        "FreestyleImage","FreestyleGeometry", "FreestyleSceneGraph", "FreestyleWingedEdge",\
                        "FreestyleViewMap", "FreestyleStroke"]

if not("FREESTYLE_DIR" in os.environ):
    print "FREESTYLE_DIR must be set to your Freestyle directory"
    sys.exit()
    
src_dir_path=os.environ['FREESTYLE_DIR']
dest_dir_path=os.path.join(os.environ['FREESTYLE_DIR'], "freestyle.2.0.0-macosx-x86")
bundle_name="Freestyle.app"
bundle_path=os.path.join(dest_dir_path,bundle_name)
frameworks_path=os.path.join(bundle_path,"Contents/Frameworks")
exe_path=os.path.join(bundle_path, "Contents/MacOS")
original_lib_path=os.path.join(src_dir_path,"build/macosx/release/lib")
original_exe_path=os.path.join(src_dir_path,"build/macosx/release",bundle_name,"Contents/MacOS")
                               


# Builds a dictionary of dependencies for
# a given binary
# The table format is:
# "dependency name" "dependency path"
def buildDependenciesTable(binary_file, dep_table):
    cmd="otool -L %s" % binary_file
    #print cmd
    #otool_output = os.system(cmd)
    otool_output = os.popen(cmd).read().split('\n')
    for dep_text in otool_output:
        if (dep_text.count(":") == 0):
            dep = dep_text.split(' ')[0].lstrip()
            dep_base_name=os.path.basename(dep)
            dep_table[dep_base_name] = dep
    

def fixPaths(dep_table):
    for k,v in dep_table.items():
        if(k.count("Freestyle")):
            dep_table[k] = os.path.join(src_dir_path, "build/macosx/release/lib",v)
        if(k.count("QGLViewer")):
            dep_table[k] = os.path.join("/usr/lib", v)

def extractFrameworkBaseDir(framework_lib):
    parts=framework_lib.split("/")
    head="/"
    tail=""
    in_head=True
    for p in parts:
        if(in_head == True):
            head=os.path.join(head,p)
        else:
            tail=os.path.join(tail,p)
        if(p.count(".framework") != 0):
            in_head=False
    return (head,tail)    
    
def installDependencies(dep_table, install_dependencies, new_dep_table):
    for k,v in dep_table.items():
        for d in install_dependencies:
            if(k.count(d)!=0):
                framework_dir_path=v
                cp_option=""
                head=""
                tail=""
                if(v.count("framework")):
                    (head,tail) = extractFrameworkBaseDir(v)
                    framework_dir_path=head
                    cp_option="-R"
                lib_name=os.path.split(framework_dir_path)[1]
                target=os.path.join(frameworks_path,lib_name)
                # update new table
                if(tail != ""):
                    new_dep_table[k] = os.path.join("@executable_path/../Frameworks",lib_name,tail)
                else:
                    new_dep_table[k] = os.path.join("@executable_path/../Frameworks",lib_name)
                if(os.path.exists(target) != True):    
                    cmd = "cp %s %s %s" % (cp_option, framework_dir_path,frameworks_path)
                    print "Installing dependency:",lib_name
                    os.system(cmd)

def updatePathsToDependencies(binary_file, install_dependencies, dep_table, new_dep_table):
    # executable:
    f_dep_table={}
    buildDependenciesTable(binary_file,f_dep_table)
    for k,v in f_dep_table.items():
        # is k in install_dependencies?
        for ld in install_dependencies:
            if(k.count(ld) != 0):
                #print new_dep_table
                cmd="install_name_tool -change %s %s %s" % (v,new_dep_table[k], binary_file)
                os.system(cmd)
    # check
    cmd="otool -L %s" % binary_file
    os.system(cmd)

def cleanDir(dir, to_delete):
    os.chdir(dir)
    #print os.getcwd()
    for d in os.listdir("."):
        #print d
        if(d == "Headers"):
            cmd="rm -rf Headers"
            to_delete.append(os.path.join(dir,d))
            #os.system(cmd)
        elif(d.count("debug") != 0):
            cmd="rm -rf %s"%(d)
            #print cmd
            to_delete.append(os.path.join(dir,d))
            #os.system(cmd)
        elif(os.path.isdir(d) == True):
            #print d
            cleanDir(os.path.join(dir,d), to_delete)
        #else:
        #    print d
    os.chdir(os.path.join(dir,".."))
    #print os.getcwd()

    
# build bundle structure
if( os.path.exists(dest_dir_path) != True):
    print "Creating directory",dest_dir_path
    os.mkdir(dest_dir_path)

if(os.path.exists(bundle_path) != True):
    print "Creating the bundle structure", bundle_path
    cmd = "cp -R %s %s" % (os.path.join(src_dir_path, "build/macosx/release/",bundle_name), bundle_path)
    os.system(cmd)
    os.mkdir(os.path.join(bundle_path,"Contents/Frameworks"))
  
                
dep_table = {}
new_dep_table = {}
# Executable
for f in os.listdir(original_exe_path):
    if(f[0] == '.'):
        continue
    exe_file_path=os.path.join(original_exe_path, f)
    buildDependenciesTable(exe_file_path, dep_table)
    
# Frameworks    
for f in os.listdir(original_lib_path):
    if (f.count("framework") == 0):
        continue
    f_name=f.split('.')[0]
    fwk_path=os.path.join(original_lib_path, "%s.framework" % f_name,f_name)
    buildDependenciesTable(fwk_path, dep_table)
                               
# Fix ad-hoc paths
fixPaths(dep_table)
    
# install dependent libs
installDependencies(dep_table, install_dependencies, new_dep_table)

# update paths to installed dependencies
for f in os.listdir(exe_path):
    if(f[0] == '.'):
        continue
    updatePathsToDependencies(os.path.join(exe_path,f), install_dependencies, dep_table, new_dep_table)
    
# Frameworks    
for f in os.listdir(frameworks_path):
    if (f.count("framework") == 0):
        continue
    f_name=f.split('.')[0]
    fwk_path=os.path.join(frameworks_path, "%s.framework" % f_name,f_name)
    updatePathsToDependencies(fwk_path, install_dependencies, dep_table, new_dep_table)
    

# Clean-up
# Remove debug libs
print "Cleaning..."
to_delete=[]
cleanDir(bundle_path, to_delete)
for f in to_delete:
    cmd = "rm -rf %s"%f
    print cmd
    os.system(cmd)
