import os, shutil, zipfile

def copy(bin_dir: str, package_dir: str, endswith: str = "*", dir: bool = False, c_dir: bool = True) -> None:
    for file_name in os.listdir(bin_dir):
        file_path = os.path.join(bin_dir, file_name)
        if os.path.isfile(file_path):
            if endswith == "*":
                shutil.copy(file_path, package_dir)
            elif file_name.endswith(endswith):
                shutil.copy(file_path, package_dir)
        elif dir:
            if not os.path.exists(os.path.join(package_dir, file_name)) and c_dir:
                os.mkdir(os.path.join(package_dir, file_name))
                copy(file_path, os.path.join(package_dir, file_name), endswith, dir)
            else:
                copy(file_path, package_dir, endswith, dir, False)

current_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(current_dir)
build_dir = os.path.join(parent_dir, "build")
bin_dir = os.path.join(build_dir, "bin")
src_dir = os.path.join(parent_dir, "src")
plugin_dir = os.path.join(src_dir, "plugin")
utils_dir = os.path.join(plugin_dir, "Utils")

if os.path.exists(bin_dir):
    package_dir = os.path.join(build_dir, "package")
    sdk_dir = os.path.join(package_dir, "SDK")
    if not os.path.exists(package_dir):
        os.makedirs(package_dir)
    if not os.path.exists(sdk_dir):
        os.makedirs(os.path.join(sdk_dir, "include"))
        os.makedirs(os.path.join(sdk_dir, "lib"))
    copy(bin_dir, package_dir, "*", True)
    copy(parent_dir, package_dir, ".md")
    copy(os.path.join(plugin_dir, "Include"), os.path.join(sdk_dir, "include"), ".h", True, False)
    copy(os.path.join(build_dir, "windows"), os.path.join(sdk_dir, "lib"), ".lib", True, False)
    shutil.copy(os.path.join(plugin_dir, "ExportLib.h"), os.path.join(sdk_dir, "include"))
    shutil.copy(os.path.join(utils_dir, "SQLiteStorage.h"), os.path.join(sdk_dir, "include"))
    shutil.copy(os.path.join(parent_dir, "LICENSE"), package_dir)
    with zipfile.ZipFile(os.path.join(build_dir, "LOICollectionA-windows-x64.zip"), "w", zipfile.ZIP_DEFLATED) as zipf:
        for root, dirs, files in os.walk(package_dir):
            for file in files:
                zipf.write(os.path.join(root, file), os.path.relpath(os.path.join(root, file), package_dir))
    shutil.rmtree(package_dir)
    print("Package created successfully.")
else:
    print("Build directory not found.")