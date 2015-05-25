#include "get.hpp"

using namespace std;

std::string prefix = "/proot/";
std::string pkg_dir = "/etc/packages/";
std::string postfix = ".uncompress";
std::string install_dir = "/packages/";
std::string conf_dir = "/etc/local/";
std::string conf_symlink = "etc";

static char version[]="Packages installation tool. Version 1.0.1b (C) Denis V. Meltsaykin 2014 E_BSD";

std::string exec_out (std::string cmd){
	FILE* pipe = popen (cmd.c_str(),"r");
	
	if (!pipe) return NULL;
	char buffer[128];
	std::string result ="";
	while (!feof (pipe)){
		if(fgets(buffer, 128, pipe) != NULL)
			result+=buffer;
	};
	pclose(pipe);
	return (result);
};

int md5_is_valid (std::string filename){
	/* using libmd */
	
	std::string md5_tmp;
	char buf[129]; /* It is set in original md5.c */
	char MD5_from_file[32], MD5_computed[32];
	
	ifstream MD5_FILE (pkg_dir + filename + ".md5");
	if (!MD5_FILE) return 0;
	MD5_FILE.get(MD5_from_file, 33);
	MD5_FILE.close();
#ifdef DEBUG
	cout << "\n>>" << MD5_from_file << "<<MD5_from_file\n";
#endif
	/* MD5File is function from libmd, confusing */

	md5_tmp = MD5File ((pkg_dir + filename).data(), buf);

#ifdef DEBUG
	cout << "\n>>" << md5_tmp.data() << "<<MD5_computed\n";
#endif	
	if (strnstr (MD5_from_file, md5_tmp.data(), 32) != NULL) 
		return 1;
	return 0;
};

int get_md (std::string *md){ /* get contents of geom config */ 
	char buf[4096];
	size_t bufsize = sizeof (buf);	
	sysctlbyname ("kern.geom.conftxt",&buf, &bufsize, NULL, 0);
	*md += buf;
	return 1;
};

int	get_mounts (std::string *mp){ /* get contents of mount */
	*mp += exec_out ("mount -v");
	return 1;
};

std::string found_in_md (std::string filename, std::string md){

	std::smatch match;
	std::regex exp (".*MD.*(md\\d+).*("+filename+").*");
	
	if(std::regex_search (md, match, exp))
		return std::string(match[1]);
	return std::string("");	
};

int found_in_mounted (std::string mp, std::string md){
	std::smatch match;
	std::regex exp (".*" + md + postfix + "\\s.*");
	if(std::regex_search (mp, match, exp)) return 1;
	return 0;
};

int PK_dirlist (std::string directory, std::list<std::string> *list){

	std::regex exp (".*pack$");
	std::smatch match;
	int count = 0;
	struct dirent *ent;
	DIR *dir = opendir (directory.data());
	if (dir != NULL) {
		while ((ent = readdir (dir)) != NULL){
			if (ent->d_type == DT_REG) /* regular file */
				if (std::regex_search(std::string(ent->d_name), match, exp)){	
					/* that is insane, using regexp to match files *.pack ?! 
					** but i don't know how to do that in other way	*/
					list->push_back(std::string(ent->d_name));
					count++;
				}
		}
		closedir (dir);
	};
	
	return count;
};

int MP_dirlist (std::string directory, std::list<std::string> *list){ 
	/* fills list of mountpoints for packages */
	int count = 0;
	struct dirent *ent;
	DIR *dir = opendir (directory.data());
	if (dir != NULL) {
		while ((ent = readdir (dir)) != NULL){
			if (ent->d_type == DT_DIR) /* directory */
				if (!strchr(ent->d_name, '.')) {	/* we don't need .. and . in list */
					list->push_back(std::string(ent->d_name));
					count++;
				}
		}
		closedir (dir);
	};
	return count;
};



std::string mounting (std::string what, std::string mp, std::list<std::string> *mount_points){

	std::list<std::string>::iterator it;
	std::string point = "";
	
	std::string command = "mount_cd9660 /dev/" + what + postfix + " " + prefix;
	
	short count = 0;
rollover:
#ifdef DEBUG
	cout << "Cycle: " << count << "\n";
#endif
	for (it = mount_points->begin(); it != mount_points->end(); it++){
		if (mp.find(prefix + *it) == std::string::npos){
#ifdef DEBUG
			cout << prefix + *it << " is free\n";
#endif
			point = *it;
			mount_points->erase(it); /* we took that */
			goto bail_out; /* NOWAI */
		} else {			
			/* we should start again if mount point not free
			** and of course start a new cycle, thus list is changed */
#ifdef DEBUG
			cout << prefix + *it << " not free\n";
#endif
			mount_points->erase(it);
			count++;
			goto rollover; /* nasty :) */
		};
	};
	
bail_out:
		command+=point;
	
	/* we found a mount point and ready to go! */

#ifdef DEBUG	
	cout << command;
#endif	
	exec_out (command);
	return prefix + point;
};

std::string make_md (std::string filename){
	std::string result = exec_out("mdconfig -f " + pkg_dir + filename + " 2>/dev/null");
	result.pop_back(); // of course!
	return result; 
};

int check_and_mount (std::list<std::string> filelist, std::string mp, std::string md, std::list<std::string> *mount_points){
	std::list<std::string>::iterator it;
	std::string md_founded;
	std::string mounted;
	for (it = filelist.begin(); it != filelist.end(); it++){
		cout << "TEST: " << *it << "... ";
		if (!md5_is_valid(*it)) {
			cout << " MD5 Signature failed. Possibly broken package!\n";
			continue;
		};
		
		cout << " MD5 Signature OK. ";
		
		md_founded = found_in_md (*it, md); 
		if (md_founded != "") {
			cout << *it << " is " << md_founded;
			if (found_in_mounted(mp, md_founded)){
				cout << " and mounted. Skip.\n";
			} else {
				cout << " and not mounted. Mounting.";
				mounted = mounting (md_founded, mp, mount_points);
				cout << " Mounted in " << mounted << ".\n";

			};
		} else {
			cout << "no device. Creating and mounting... ";
			md_founded = make_md (*it);
			cout << "MD: " << md_founded;
			mounted = mounting (md_founded, mp, mount_points);
			cout << " Mounted in " << mounted << ".\n";
		};
			
	};
	
	return 1;
};

int traverse_dir (std::string start_dir, std::string jump = ""){
	struct dirent *ent;
	struct stat item_stat;
	struct stat tmp_stat;
	
	start_dir+=jump;
#ifdef DEBUG
	cout << "Traverse "<< start_dir << "\n";
#endif
	DIR *dir = opendir ((start_dir).data());
	if (dir != NULL) {
			while ((ent = readdir (dir)) != NULL){
				stat (std::string(start_dir + std::string(ent->d_name)).data(), &item_stat);
				if (S_ISDIR(item_stat.st_mode)){
					if (ent->d_name[0] != '.'){
						if (stat ((install_dir + jump + ent->d_name).data(), &tmp_stat)){
							/* dir in install_dir not exist */
							cout << " Creating " << install_dir + jump + ent->d_name << "\n";
							if (mkdir((install_dir + jump + ent->d_name).data(), 0777) != 0)
								cout << "mkdir(" << install_dir + jump + ent->d_name <<"): fail!\n";
						} 
						else {
#ifdef DEBUG
							cout << " EXISTS: " << install_dir + jump + ent->d_name << "\n";
#endif
						}
						traverse_dir (start_dir , jump+std::string(ent->d_name) + "/");
					}
				} else {
#ifdef DEBUG	
				cout << jump << ent->d_name << " is a file |\n";
#endif			
				unlink ((install_dir + jump + ent->d_name).data()); /* drop anyway */
				symlink ((start_dir + ent->d_name).data(), (install_dir + jump + ent->d_name).data());
				};
			};

		
		closedir (dir);
	};
	return 1;
};

int populate_links (std::string directory) {
	/* Routine to populate symlinks 
	* /proot/a/bin/file -> /packages/bin/file */
	int count = 0; 
	struct dirent *ent;
	struct stat item_stat;
	
#ifdef DEBUG
	cout << ">in: " << directory ;
#endif		
		DIR *dir = opendir (directory.data());
		if (dir != NULL) {
			while ((ent = readdir (dir)) != NULL){
				stat (std::string(directory + std::string(ent->d_name)).data(), &item_stat);
				if (S_ISDIR(item_stat.st_mode)){
					if (! strchr(ent->d_name, '.')){
						traverse_dir (directory + std::string(ent->d_name) + "/");
						count++;
					}
				} else {
#ifdef DEBUG	
				cout << directory << ent->d_name << " is a file |\n";
#endif			

				};
			};
			closedir (dir);

		};

	return 1;
};

int create_conf_symlink() {
    cout << "Creating config-store from "+conf_dir+" to "+install_dir+conf_symlink+" ...";
    if (symlink((conf_dir).data(), (install_dir + conf_symlink).data()) != 0) {
	cout << "FAILED!\n";
	return 1;
    };
    cout << "OK\n";
    return 0;
};

int usage (){
	cout << version << "\n"\
	"Usage:\n\n"\
	"  -i --install-dir\tDirectory in which packages should be installed\n"\
	"\t\t\t\tdefault is: " + install_dir + "\n"\
"  -p --package-dir\tDirectory where located packages (*.pack)\n"\
"\t\t\t\tdefault is: " + pkg_dir + "\n"\
"  -m --mount-dir\tBase directory to mount packages to\n"\
"\t\t\t\t(should contain subdirs a to z)\n"\
"\t\t\t\tdefault is: " + prefix + "\n"\
"  -c --conf-dir\t\tDirectory where stores configs\n"\
"\t\t\t\tdefault is: " + conf_dir + "\n\n"\
" This software requires mount, mount_cd9660, mdconfig, sysctl and\n"\
" root permissions in order to work properly.\n\n";
	return 0;
};	

int main (int argc, char ** argv) {



	int c;
	while (1){
		static struct option long_options[] = {
			{"install-dir", required_argument, 0, 'i'},
			{"package-dir", required_argument, 0, 'p'},
			{"mount-dir", required_argument, 0, 'm'},
			{"conf-dir", required_argument, 0, 'c'},
			{"help", no_argument, 0, 'h'},
			{0,0,0,0}
		};
		int option_index=0;
		c = getopt_long (argc, argv, "i:p:m:c:h", long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
			case 'i':
				install_dir = optarg;
				if (install_dir.front() != '/') install_dir.insert(0, "/");
				if (install_dir.back() != '/') install_dir.push_back('/');
				break;
			case 'p':
				pkg_dir = optarg;
				if (pkg_dir.front() != '/') pkg_dir.insert(0, "/");
				if (pkg_dir.back() != '/') pkg_dir.push_back('/');
				break;
			case 'm':
				prefix = optarg;
				if (prefix.front() != '/') prefix.insert(0, "/");
				if (prefix.back() != '/') prefix.push_back('/');
				break;
			case 'c':
				conf_dir = optarg;
				if (conf_dir.front() != '/') conf_dir.insert(0, "/");
				if (conf_dir.back() != '/') conf_dir.push_back('/');
				break;

			case '?':
				cout << "Use -h or --help to see usage guide.\n";
				return -1;
			case 'h':
				usage();
				return -2;
			default:
				abort();
		};
	};
	cout << version << "\n";
	create_conf_symlink();
	cout << "Installing from ["+pkg_dir+"] to ["+prefix+"] linking to ["+install_dir+"]\n";

	
	std::list<std::string> filelist;
	std::list<std::string> dirlist;	
	std::list<std::string>::iterator it;
	std::string mp,md;


	PK_dirlist (pkg_dir, &filelist);
	MP_dirlist (prefix, &dirlist);
#ifdef DEBUG
	for (it = dirlist.begin(); it != dirlist.end(); it++)
		cout << *it << " || ";
#endif
	get_mounts (&mp);
	get_md (&md);
	
	check_and_mount (filelist, mp, md, &dirlist);

#ifdef DEBUG	
	std::smatch match;
	std::regex exp (".*\\./get.*");
	std::regex_search (exec_out("/bin/ps auxw"), match, exp);
	cout << match.str() << "\n";
#endif
	populate_links (prefix);
	return 0;
};
