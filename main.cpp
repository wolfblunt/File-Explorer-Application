#include<iostream>
#include<stdio.h>
#include<string>
#include<string.h>
#include<vector>
#include<stdlib.h>
#include<termios.h>
#include<unistd.h>
#include<ctype.h>
#include<limits.h>
#include<dirent.h>
#include<sys/stat.h>
#include<algorithm>
#include<iomanip>
#include<sys/ioctl.h>
#include<stack>
#include<pwd.h>
#include<grp.h>
#include<signal.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<fstream>
#include<fcntl.h>
#include <cmath>
#include <limits.h>
#include <malloc.h>
#include <unistd.h>
#include <cstdlib>

using namespace std;


string runningDirPath;  // stores the running Directory Path
int start_record_ind;
bool normal_file_mode=true;
bool createNormalModeScreen=true;  // flag for creating Normal Screen Mode Configuration
bool quit=true;  // flag to quit the program
int positionX=1;  // maintaing the coordinate details
bool content_info_refresh=true; //need_to refresh the content=true
struct termios original_config;

vector<int> contentMaxLength(7);
vector<bool> isDirectory;
vector<string> curr_path; // current path vector - stores the path address in array
vector<string> currPathContent;  // contains all curr path content
vector<vector<string>> curr_path_content_info; // vector contains current directory content information (i.e path, permission, etc)
stack<vector<string>> forward_history;
stack<vector<string>> backward_history;


void moveCursor(int x, int y) {
    //cout<<"\033[1;7m";
    cout<<"\x1b["<<x<<";"<<y<<"H";
    //cout<<"\033[0;7m";
}

void stringToCharArr(string input, char ** output){
    int inputStringLength = input.length();

    *output = new char[inputStringLength+1];
    strcpy(*output, input.c_str());
}

void stringToVector(string &input, vector<string> &output){
    string temp;
    output.clear();

    int inputStringLength = input.length();
    
    int i=1;
    while(i<inputStringLength)
    {
        char ithCharacter=input[i];

        if(ithCharacter == '/'){
            output.push_back(temp);
            temp.clear();
        }else{
            temp.push_back(ithCharacter);
        }
        i++;
    }

    if(input[inputStringLength-1] != '/'){
        output.push_back(temp);
    }
}

void diableNonCanonicalMode(){
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_config);
}

void vectorToString(vector<string> & input, string & output){
    for(auto i=input.begin(); i!= input.end(); i++)
    {
        output.push_back('/');
        output.append(*i);
    }
}

// void getStartPath(string & output){
//     char path[4096];
//     getcwd(path, sizeof(path));

//     output = path;
// }

void enableNonCanonicalMode(){
    tcgetattr(STDIN_FILENO, &original_config); // to read the current attributes into a struct
    atexit(diableNonCanonicalMode); // function to be called on normal program termination.

    struct termios new_config = original_config;
    //new_config.c_lflag = ~(ECHO | ICANON);
    new_config.c_lflag = ~(ECHO | ICANON | IXON | ISIG); // disables ctrl+v , ctrl+s, ctrl+q and etc.

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_config);
}

void getCursorContent(){
    // this function fetched the contents of the current directory in sorted order 

    start_record_ind=0;
    positionX=1;

    isDirectory.clear();
    currPathContent.clear();
    curr_path_content_info.clear();   

    string path_str;
    vectorToString(curr_path,path_str);

    char * path_char_ptr;
    stringToCharArr(path_str, &path_char_ptr);

    struct dirent *dir_entry_ptr;
    DIR *dir_ptr = opendir(path_char_ptr);

    while ((dir_entry_ptr = readdir(dir_ptr)))
    {
        currPathContent.push_back(dir_entry_ptr->d_name);
    }    
    closedir(dir_ptr);

    sort(currPathContent.begin(), currPathContent.end());
}

void ownershipDetails(vector<string> &currentContentInfo, struct stat &file_info)
{
    // calculate the ownership details for particular content in the directory
    const int owner = 2;
    const int groupOwner = 3;
    struct passwd *pw = getpwuid(file_info.st_uid);

    string user_str = pw->pw_name;
    currentContentInfo.push_back(user_str);

    int max_len = (user_str).length();
    if(max_len > contentMaxLength[owner]){ 
        contentMaxLength[owner] = max_len;
    }

    struct group *gr = getgrgid(file_info.st_gid);

    string grp_str = gr->gr_name;
    currentContentInfo.push_back(grp_str);

    max_len = (grp_str).length();
    if(max_len > contentMaxLength[groupOwner]){ 
        contentMaxLength[groupOwner] = max_len;
    }
}

void sizeContent(vector<string> &currentContentInfo, struct stat &file_info)
{
    // calculate the size for particular content in the directory
    const int s = 1;
    int size = file_info.st_size;
    string size_str;
    if(size>1024)
    {
        int siz = ((double)size) / 1024;
        size_str = to_string(siz);
        size_str.push_back('K');
    }
    else{
        size_str = to_string(size);
        size_str.push_back('B');
    }
    while(size_str.back()=='0')
    {
        size_str.pop_back();
    }
    if(size_str.back()=='.')
    {
        size_str.push_back('0');
    }
    currentContentInfo.push_back(size_str);
    
    int max_len = (size_str).length();
    if(max_len > contentMaxLength[s]){ 
        contentMaxLength[s] = max_len;
    }
}

void directoryContentPermission(vector<string> &currentContentInfo, struct stat &file_info)
{
    // calculate the permission for particular content in the directory

    string permCheck;  // storing the permission

    S_ISDIR(file_info.st_mode) ? permCheck.push_back('d') : permCheck.push_back('-') ;

    file_info.st_mode & S_IRUSR ? permCheck.push_back('r') : permCheck.push_back('-') ;
    file_info.st_mode & S_IWUSR ? permCheck.push_back('w') : permCheck.push_back('-') ;
    file_info.st_mode & S_IXUSR ? permCheck.push_back('x') : permCheck.push_back('-') ;

    file_info.st_mode & S_IRGRP ? permCheck.push_back('r') : permCheck.push_back('-') ;
    file_info.st_mode & S_IWGRP ? permCheck.push_back('w') : permCheck.push_back('-') ;
    file_info.st_mode & S_IXGRP ? permCheck.push_back('x') : permCheck.push_back('-') ;

    file_info.st_mode & S_IROTH ? permCheck.push_back('r') : permCheck.push_back('-') ;
    file_info.st_mode & S_IWOTH ? permCheck.push_back('w') : permCheck.push_back('-') ;
    file_info.st_mode & S_IXOTH ? permCheck.push_back('x') : permCheck.push_back('-') ;

    currentContentInfo.push_back(permCheck);
}

void directoryContentModifiedDetails(vector<string> &currentContentInfo, struct stat &file_info)
{
    // struct tm last_modified_time_info= *(gmtime(&file_info.st_mtime));  // last modified

    string data_time = string(ctime(&file_info.st_mtime));
    data_time.pop_back();    
    currentContentInfo.push_back(data_time);
}

void getCurContentInfo(){
    // this function calculates all the metadata details of the current directory contents and stores them into 
    // curr_path_content_info vector
    
    const int NAME = 0;

    // for getting the width
    contentMaxLength[0]=INT_MIN; //name
    contentMaxLength[1]=INT_MIN; //size
    contentMaxLength[2]=INT_MIN; //owner
    contentMaxLength[3]=INT_MIN; //groupOwner

    contentMaxLength[4]=10; //permissions
    contentMaxLength[5]=10; //lastModifiedDate
    contentMaxLength[6]=8; //lastModifiedTime

    string path_str;
    vectorToString(curr_path,path_str);
    for(auto i=currPathContent.begin(); i!= currPathContent.end(); i++)
    {

        string content_path_str = path_str + "/" + (*i);
        char * content_path_char;

        stringToCharArr(content_path_str, &content_path_char);

        struct stat file_info;
        stat(content_path_char, &file_info);

        vector<string> currentContentInfo;
        int max_len;

        currentContentInfo.push_back(*i);   // File Name

        sizeContent(currentContentInfo, file_info);  // Size
        
        ownershipDetails(currentContentInfo,file_info);  // Owner
        
        isDirectory.push_back(S_ISDIR(file_info.st_mode));   // storing type of content in bool vector and referring while printing dir contents
        
        directoryContentPermission(currentContentInfo,file_info);   // Permission
 
        directoryContentModifiedDetails(currentContentInfo,file_info);   // Date and Time

        
        max_len = (*i).length();
        if(max_len > contentMaxLength[NAME])
        { 
            contentMaxLength[NAME] = max_len;
        }

        curr_path_content_info.push_back(currentContentInfo);   // Push the file/DIR information in vector
    }
}

void printCurrPathContentInfo(){
    // this function prints current directory contents with its metadata

    struct winsize curr_win_size;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &curr_win_size);
    int max_record_count = curr_win_size.ws_row;
    max_record_count--;

    // normal data printing
    int total_record=curr_path_content_info.size();
    int loop_end= max_record_count+start_record_ind-1;

    if(loop_end > total_record){
        loop_end=total_record-1;
    }

    for(int i=start_record_ind;i<loop_end+1;i++)
    {

        int ind=0;
        auto contentInfo = curr_path_content_info[i].begin();
        while(contentInfo!=curr_path_content_info[i].end())
        {
            cout<<setw(contentMaxLength[ind])<<left<<*contentInfo<<"\t";
            contentInfo++;
            ind++;
        }
        cout<<"\n";
    }

    // Prints Current Directory
    moveCursor(max_record_count+1,1);
    string curr_path_str;

    vectorToString(curr_path,curr_path_str);
    cout<<"Status-bar : "<<curr_path_str<<" ";
    
}

void checkForResize(int sig){

    int t_x_cord = positionX;

    sleep(1);

    struct winsize curr_win_size;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &curr_win_size);
    int max_record_count = curr_win_size.ws_row;
    max_record_count--;

    if(normal_file_mode){

        cout<<"\x1b[2J";  // clear screen and send cursor to home position
        cout<<"\x1b[H";   // send cursor to cursor home position.

        printCurrPathContentInfo();

        if(t_x_cord > max_record_count){
            positionX = 1;
            cout<<"\x1b[H";
        }else{
            moveCursor(positionX,1);   // moving the cursor to cordinate X = positionX and and Y cordinateY = 1 
        }
    }
}


void NormalModeSetting()
{
    cout<<"\x1b[2J";
    cout<<"\x1b[H";

    if(content_info_refresh){
        getCursorContent();
        getCurContentInfo();
    }
    printCurrPathContentInfo(); // to print directory contents
}

void fileOpen(string file_path)
{
    string checkFileType = file_path.substr(file_path.size()-3, 3);

    char * file_path_char_ptr;
    stringToCharArr(file_path, &file_path_char_ptr);

    const char *file_or_url = file_path.c_str();
    pid_t pid = fork();
    string fileType = file_path.substr(file_path.size()-3);

    if(fileType=="cpp" || fileType=="txt" || fileType=="out" || fileType=="css")
    {
        if(pid==0){
            execl("/usr/bin/vi","vi",file_path_char_ptr,NULL);
            exit(0);
        }
    }
    else
    {
        if(pid==0){
            execlp("xdg-open", "xdg-open", file_path_char_ptr, (char *)0);
            exit(0);
        }
    }
    wait(NULL);
}

void UpArrowKeyFunction()
{
    // Jab kisi ne Up arrow key press ki
    // In this function we are moving the cursor upwards
    if(positionX>1){
        moveCursor(positionX,1);
        cout<<"\x1b[A"; //<esc>[A
        //cout<<"\033[1;7m";
        createNormalModeScreen=false;
        positionX--;
    }
    else{
        if(start_record_ind == 0 ){
            createNormalModeScreen=false;
        }else{
            start_record_ind--;
        }
    }
}

void DownArrowKeyFunction(int& max_record_count, int& total_record)
{
    // Jab kisi ne down arrow key press ki
    // In this function we are moving the cursor downward
    if(positionX<max_record_count && positionX+start_record_ind<total_record){
        moveCursor(positionX,1);
        cout<<"\x1b[B";
        // cout<<"\033[1;7m";
        //positionX++;
        //NormalModeSetting();
        createNormalModeScreen=false;
        positionX++;
    }
    else if(positionX+start_record_ind==total_record) // Case when total number of records equal to terminal page size
    {
        createNormalModeScreen=false;
    }
    else  // Case when number of records don't fit in terminal page
    {
        if(start_record_ind + max_record_count == total_record )
        { 
            createNormalModeScreen=false;
        }
        else
        {
            start_record_ind++;
        }
    }
}

void RightArrowKeyFunction()
{
    // Jab kisi ne right arrow key press ki
    // Is function mai ham forward history par ja raha hai
    if(!forward_history.empty()){
        backward_history.push(curr_path);
        curr_path=forward_history.top();
        forward_history.pop();

        positionX=1;
        start_record_ind=0;
    }
    else{
        createNormalModeScreen=false;
        content_info_refresh=false;
    }
}

void LeftArrowKeyFunction()
{
    // Jab kisi ne left arrow key press ki
    // Is function mai ham backward history par ja raha hai
    if(!backward_history.empty()){
        forward_history.push(curr_path);
        curr_path=backward_history.top();
        backward_history.pop();

        positionX=1;
        start_record_ind=0;
    }
    else{
        createNormalModeScreen=false;
        content_info_refresh=false;
    }
}

void EnterKeyFunction()
{
    // Jab kisi ne Enter Key Press ki
    // Enter hone par:
    //    - if File - we are opening the file
    //    - if Folder - we are going inside the folder and printing its content 
    // Enter ASCII code : 13
    string nextAddress = currPathContent[positionX + start_record_ind-1];
    if(nextAddress[0]=='.')
    {
        int nextAddresslength = nextAddress.length();
        if(nextAddresslength==1)
        {
            positionX=1;
            start_record_ind=0;
        }
        else if(nextAddresslength==2 && nextAddress[1] == '.')
        {
            string homecheckString;
            vectorToString(curr_path, homecheckString);
            if(homecheckString!="/home")  // agar ham home path pe nahi hai toh because after home path nothing is there - Avoiding Segmentation Fault
            {
                backward_history.push(curr_path);
                curr_path.pop_back();
                positionX=1;
                start_record_ind=0;
            }
        }
        else{
            if(isDirectory[positionX+start_record_ind-1])
            {

                backward_history.push(curr_path);

                curr_path.push_back(nextAddress);
                positionX=1;
                start_record_ind=0;
            }
            else
            {
                //pid_t pid=fork();
                string file_path;
                vectorToString(curr_path,file_path);

                file_path = file_path+"/"+nextAddress;
                cout<<"FilePath : "<<file_path;

                fileOpen(file_path);
            }
        }
    }
    else{
        if(isDirectory[positionX+start_record_ind-1])
        {
            backward_history.push(curr_path);

            curr_path.push_back(nextAddress);
            positionX=1;
                start_record_ind=0;
        }
        else
        {
            string file_path;
            vectorToString(curr_path,file_path);

            file_path = file_path+"/"+nextAddress;
            cout<<"FilePath : "<<file_path;


            fileOpen(file_path);
        }
    }
    
    while(!forward_history.empty()){
        forward_history.pop();
    }
}

string fetchHomePath()
{
    string h_path;
    char *homedir = getenv("HOME");
    uid_t uid = getuid();
    struct passwd *pw = getpwuid(uid);
    h_path=pw->pw_dir;
    return h_path;
}

void HomekeyFunction()
{   
    // Jab kisi ne "h" key press ki
    // Is function mai ham home path par ja raha hai
    backward_history.push(curr_path);
    //string h_path = "/home/aman";
    string h_path;
    h_path = fetchHomePath();
    stringToVector(h_path,curr_path);     
    positionX=1;
    start_record_ind=0;

    while(!forward_history.empty()){
        forward_history.pop();
    }
}

void BackSpaceKeyFunction()
{  
    // Jab kisi ne BackSpace Key Press ki
    // Is function mai I'm implementing to go one level up means child ke parent pe ja rahe hai
    // Backspace ASCII code : 127
    cout<<"\ncurr_path :"<<curr_path[0];
    string homecheckString;
    vectorToString(curr_path, homecheckString);
    if(homecheckString!="/home")  // agar ham home path pe nahi hai toh because after home path nothing is there - Avoiding Segmentation Fault
    {
        backward_history.push(curr_path);
        curr_path.pop_back();
        positionX=1;
        start_record_ind=0;

        while(!forward_history.empty()){
            forward_history.pop();
        }
    }
}

void clearCommand()
{
    struct winsize curr_win_size;
    int max_record_count = curr_win_size.ws_row;
	int lastLine = max_record_count+1;
	printf("%c[%d;%dH",27,lastLine,1);
	printf("%c[2K", 27);
}

bool checkPathValidity(string destinationPath){

    DIR *dir_pointer;

    char * path_char_ptr;
    stringToCharArr(destinationPath, &path_char_ptr);

    dir_pointer = opendir(path_char_ptr);

    if(dir_pointer == NULL){
        return false;
    }        
    return true;
}

void pathCorrector(string &inputPath, string &outputPath)
{
    // gives the absolute path
    char *fullpath;
    if(inputPath[0] == '~')
    {
        string h_path = fetchHomePath();
        outputPath = h_path + inputPath.substr(1);
        fullpath = realpath(outputPath.c_str(), NULL);  // realpath will convert the relative part to absolute part
        if(fullpath!=NULL)
        {
            string outputPath1(fullpath);
            outputPath = outputPath1;
        }
    }
    else if(inputPath[0] == '/')
    {
        fullpath = realpath(inputPath.c_str(), NULL);
        if(fullpath!=NULL)
        {
            string outputPath1(fullpath);
            outputPath = outputPath1;
        }
        // string outputPath1(fullpath);
        // outputPath = outputPath1;
    }
    else if(inputPath[0] == '.')
    {
        char *fullpath;
        string cPath;
        vectorToString(curr_path, cPath);
        cPath +="/"+inputPath;
        //string tempPath = outputPath + "/" + inputPath;
        fullpath = realpath(cPath.c_str(), NULL);
        if(fullpath!=NULL)
        {
            string outputPath1(fullpath);
            outputPath = outputPath1;
        }
        // string outputPath1(fullpath);
        // outputPath = outputPath1;
    }
    else
    {
        string tempPath;
        vectorToString(curr_path, tempPath);
        tempPath = tempPath + "/" + inputPath;
        char *fullpath;
        fullpath = realpath(tempPath.c_str(), NULL);
        if(fullpath!=NULL)
        {
            string outputPath1(fullpath);
            outputPath = outputPath1;
        }
        // string outputPath1(fullpath);
        // outputPath = outputPath1;
    }
}

bool searchStatus(string searchKeyword, string pathToSearch)
{
    // recursive function which provides the search status of a directory/file in a pwd
    // 0 - Not Present
    // 1 - Present
    if(chdir(pathToSearch.c_str()) == -1){
		return 0;
	}
    struct stat file_info;
    DIR * dirContent = opendir(pathToSearch.data());
	struct dirent *dir_ptr = readdir(dirContent);
    while(dir_ptr!=NULL)
    {
        if(dir_ptr->d_name[0]!='.')
        {
            stat(dir_ptr->d_name,&file_info);
			//cout<<"Searching in ... "<<currDir<<"/"<<dir_ptr->d_name<<endl;
            if(dir_ptr->d_name == searchKeyword){
				return 1;
			}
            if(S_ISDIR(file_info.st_mode))
            {
                string innerDirPath = pathToSearch; 
                innerDirPath = innerDirPath + "/" + dir_ptr->d_name;
                if(searchStatus(searchKeyword,innerDirPath))
                {
					return 1;
                }
            }
        }
        dir_ptr = readdir(dirContent);
    }
    return 0;
}

void createFile(string destinationPath)
{
    // Flags: S_IRUSR - provides read permission to user
    //        S_IWUSR - provides write permission to user
    //        S_IRGRP - provides read permission to group
    //        S_IWGRP - provides write permission to group
    //        S_IROTH - provides read permission to others
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
    creat(destinationPath.c_str(),mode);
    cout<<"\nFile Created Successfully\n";
}

void createDirectory(string destinationPath)
{
    // Flags: S_IRUSR - provides read permission to user
    //        S_IWUSR - provides write permission to user
    //        S_IRGRP - provides read permission to group
    //        S_IWGRP - provides write permission to group
    //        S_IROTH - provides read permission to others
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
    //path newDestinationPath = destinationPath;
    // fs::create_directories("home/aman/dumps");
    mkdir(destinationPath.c_str(),mode);
    cout<<"\nFolder Created Successfully\n";
}

void createDirectoryFromSourceWithPermission(string destinationPath, string sourcePath)
{
    char * sourcePathChar;
    stringToCharArr(sourcePath, &sourcePathChar);

    struct stat file_info;
    stat(sourcePathChar, &file_info);

    __mode_t permission=file_info.st_mode;
    mkdir(destinationPath.c_str(),permission);  // Creating directory with same permission as source directory 
}

void gotoFunctionality(string goTopath, string currentPath, string tempCurrPath)
{
    // Input 1: goto ~ -> Change pwd to home directory
    // Input 2: goto /home/aman/Videos  -> Change pwd to videos folder
    // Input 3: goto hello -> Change pwd to CurrDirectoryPath/hello
    // Input 4: goto ./hello -> Change pwd to CurrDirectoryPath/hello
    
    bool is_valid_path;
    is_valid_path = checkPathValidity(tempCurrPath);

    if(is_valid_path){
        currentPath = tempCurrPath;
        stringToVector(tempCurrPath,curr_path);
        // added to show goto directory content
        content_info_refresh=true;  // making it true to update and print the new directory content
        NormalModeSetting();
        content_info_refresh=false;  // making it false again to not update the content
    }else{
        backward_history.pop();
        cout<<"\n\n'"<<goTopath<<"' - path is invalid\n";
    }
}

void copyDataFunction(string sourcePath, string destinationPath)
{
    // reading the source file content and writing it into destination file
    ifstream sourcePointer(sourcePath, ios::binary);
    ofstream destinationPointer(destinationPath, ios::binary);

    destinationPointer << sourcePointer.rdbuf();

    sourcePointer.close();
    destinationPointer.close();
}

void copyDirectoryContentFunction(string sourcePath, string destinationPath)
{
    vector<string> sourcePathContent;
    char * pathInfo;
    stringToCharArr(sourcePath, &pathInfo);
    createDirectoryFromSourceWithPermission(destinationPath, sourcePath);
    struct dirent *dir_entry_ptr;
    DIR *dir_ptr = opendir(pathInfo);

    while ((dir_entry_ptr = readdir(dir_ptr)))
    {
        sourcePathContent.push_back(dir_entry_ptr->d_name);
    }    
    closedir(dir_ptr);

    sort(sourcePathContent.begin(), sourcePathContent.end());
    auto directorycontent = sourcePathContent.begin()+2; // leaving . & ..
    while(directorycontent!=sourcePathContent.end()){
        //cout<<"\nContent: "<<*directorycontent;
        string tempSourcePath = sourcePath + "/";
        tempSourcePath += (*directorycontent);
        char * contentPathPtr;
        stringToCharArr(tempSourcePath, &contentPathPtr);
        struct stat file_info;
        stat(contentPathPtr, &file_info);
        if(S_ISDIR(file_info.st_mode)){
            string tempDestinationPath = destinationPath + "/";
            tempDestinationPath += *directorycontent;
            copyDirectoryContentFunction(tempSourcePath, tempDestinationPath);
        }else{
            string tempSourcePath1 = sourcePath + "/" + *directorycontent;
            string tempDestinationPath = destinationPath + "/" + *directorycontent;
            createFile(tempDestinationPath);
            copyDataFunction(tempSourcePath1, tempDestinationPath);
        }
        directorycontent++;
    }
}

void contentRenameFunctionality(string currFileName, string updateFileName, string currentPath)
{
    // for renaming the directory or file
    // syntax :- rename oldName newName
    // rename a1.txt a4.txt  -- for files
    // rename dir1 dir2  -> rename dir1 to dir2
    string fullCurrFileName;
    string fullupdateFileNameChar;
    char* currFileNameChar;
    char* updateFileNameChar;
    pathCorrector(currFileName, fullCurrFileName);
    pathCorrector(currentPath, fullupdateFileNameChar);
    fullupdateFileNameChar += "/"+updateFileName;
    stringToCharArr(fullCurrFileName, &currFileNameChar);
    stringToCharArr(fullupdateFileNameChar, &updateFileNameChar); // input, output
    int isRenameSuccess = rename(currFileNameChar , updateFileNameChar);
    if (isRenameSuccess == 0){
        cout<<"\nRename File Success";
    }
    else{
        cout<<"\nInvalid Create File Command";
    }
}

void removeFileFunctionality(string deleteFileName)
{
    char *deleteFileNamePtr;

    stringToCharArr(deleteFileName,&deleteFileNamePtr);

    int ifSuccess = remove(deleteFileNamePtr);
    if (ifSuccess != 0)
    {
        cout<<"\nUnable to delete";
    }
    else{
        cout<<"\nDeleted File Successfully";
    }
}

void removeDirectoryFunctionality(string deleteDirName)
{
    // delete the directory
    // syntax :- delete_dir dirName
    // ex: delete_dir Folder1
    char *deleteDirNamePtr;

    stringToCharArr(deleteDirName,&deleteDirNamePtr);

    struct dirent *dirEntryPtr;
    DIR *dir_ptr = opendir(deleteDirNamePtr);
    vector<string> DeleteDirPathContent;
    while ((dirEntryPtr = readdir(dir_ptr)))
    {
        DeleteDirPathContent.push_back(dirEntryPtr->d_name);
    }    
    closedir(dir_ptr);

    sort(DeleteDirPathContent.begin(), DeleteDirPathContent.end());
    auto delDirContent = DeleteDirPathContent.begin()+2; // handling . and ..

    while(delDirContent!=DeleteDirPathContent.end())
    {
        string contentName = deleteDirName + "/";
        contentName += (*delDirContent);
        if(!checkPathValidity(contentName))
        {
            removeFileFunctionality(contentName);
        }
        else
        {
            removeDirectoryFunctionality(contentName);
        }
        delDirContent++;
    }

    int isDeleteDirSuccess = remove(deleteDirNamePtr);
    if (isDeleteDirSuccess != 0)
        cout<<"\nUnable to delete the Directory";
    else{
        cout<<"\nDirectory successfully Deleted";
    }
}

void moveContentFunctionality(vector<string> moveContentArray, string destinationPath, string currentPath)
{
    // move the directory or file
    // syntax:- move file/dirName destinationPath
    // move a1.txt /home/aman/dumps
    auto moveSingleContent = moveContentArray.begin() + 1;
    while(moveSingleContent != (moveContentArray.end()-1))
    {
        string moveContent = currentPath + "/";
        moveContent += (*moveSingleContent);
        if(!checkPathValidity(moveContent))
        {
            string tempdestinationPath = destinationPath + "/" + *moveSingleContent;
            copyDataFunction(moveContent, tempdestinationPath);
            removeFileFunctionality(moveContent);
        }
        else
        {
            string tempdestinationPath = destinationPath + "/" + *moveSingleContent;
            //string tempdestinationPath = destinationPath;
            copyDirectoryContentFunction(moveContent, tempdestinationPath);
            removeDirectoryFunctionality(moveContent);
        }
        moveSingleContent++;
    }       
    cout<<"\nMoved Content Successful\n";
}

void CommandModeFunction()
{
    string currentPath;
    vector<string> userCommand;
    vectorToString(curr_path, currentPath);
    cout<<"\n"<<currentPath<<" >>>";
    char user_input; // input_char
    string user_input_str;
    while(((user_input = getchar())!= 13) && user_input!=27)   // Enter ASCII code : 13 and Escape ASCII code - 27
    {
        if(user_input==127)  // BackSapce Key
        {
            clearCommand();
            cout<<currentPath<<" >>>";
            if(currentPath.length()<=1)
            {
                user_input_str="";
            }
            else{
                user_input_str = user_input_str.substr(0,user_input_str.length()-1);
            }
            cout<<user_input_str;
        }
        else{
            user_input_str = user_input_str + user_input;
            cout<<user_input;
        }
    }
    if(user_input==27)
    {
        content_info_refresh=true;
        normal_file_mode=true;

        while(!forward_history.empty())
        {
            forward_history.pop();
        }
    }

    string commandStr;
    for(int i=0; i<user_input_str.length(); i++)
    {
        user_input = user_input_str[i];

        if(user_input==' '){
            if(commandStr.length() > 0){
                userCommand.push_back(commandStr);
            }
            commandStr = "";
        }else{
            commandStr.push_back(user_input);
        }
    }
    if(commandStr.length() > 0){
        userCommand.push_back(commandStr);
    }

    if(userCommand.size() > 0){
        string command_type =userCommand[0];
        
        if(!command_type.compare("search")){
            // searches the file/direcotry inside the current directory
            // syntax :- search aman.txt  (OR)  search Folder1
            // gives true if file/dir is present else return false
            if(userCommand.size()!=2)
            {
                cout<<"\nInvalid Command\n";
                userCommand.clear();
            }
            else{
                string searchKeyword = userCommand[1];
                string pathToSearch;
                vectorToString(curr_path,pathToSearch);

                if(searchStatus(searchKeyword, pathToSearch))
                {
                    cout<<"\nTrue\n";
                }
                else
                {
                    cout<<"\nFalse\n";
                }
                char * pathToSearchChar;
                stringToCharArr(pathToSearch, &pathToSearchChar);
                chdir(pathToSearchChar);  // change the path again to currDirectory path
            }
        }

        else if(!command_type.compare("create_file"))
        {
            // creates the single or multiple files
            // Single File syntax : create_file <file_name> <destination_path>
            // Ex: create_file a1.txt /home/aman/folder1
            // Multiple File syntax: create_file <file_name1> <destination_path1> <file_name2> <destination_path2>
            // Ex: create_file a1.txt /home/aman/folder1 a2.txt /home/aman/folder2

            // Single File
            if(userCommand.size()<4){
                //cout<<"\nInside IF";
                string fileName = userCommand[1];
                string createPath = userCommand[2];
                string pathToCreate;
                if(userCommand.size()!=3)
                {
                    cout<<"\nInvalid Create File Command";
                    userCommand.clear();
                }
                else{
                    if(createPath[0]=='.')
                    {
                        vectorToString(curr_path, pathToCreate);
                        createPath = pathToCreate + createPath.substr(1);
                        if(checkPathValidity(createPath))
                        {
                            createPath+="/";
                            createPath+=fileName;
                            createFile(createPath);
                        } 
                        else{
                            cout<<"\nInvalid Path\n";
                        }
                    }
                    else
                    {
                        string fullCreatePath;
                        pathCorrector(createPath, fullCreatePath);
                        if(checkPathValidity(fullCreatePath))
                        {
                            fullCreatePath+= "/"+fileName;
                            cout<<"\nfullCreatePath : "<<fullCreatePath;
                            createFile(fullCreatePath);
                        } 
                        else{
                            cout<<"\nInvalid Path\n";
                        }
                    }
                }
            }

            // For Multiple Files
            else{
                for(int i=0;i<userCommand.size()-1;i+=2)
                {
                    string fileName = userCommand[i+1];
                    //cout<<"\nFileName: "<<fileName;
                    string createPath = userCommand[i+2];
                    //cout<<"\ncreatePath: "<<createPath;
                    string pathToCreate;
                    if(!fileName.empty() && !createPath.empty())
                    {
                        if(createPath[0]=='.')
                        {
                            vectorToString(curr_path, pathToCreate);
                            createPath = pathToCreate + createPath.substr(1);
                            if(checkPathValidity(createPath))
                            {
                                createPath+="/";
                                createPath+=fileName;
                                // cout<<"FilePathCrete: "<<createPath;
                                createFile(createPath);
                            } 
                            else{
                                cout<<"\nInvalid Path\n";
                            }
                        }
                        else
                        {
                            string fullCreatePath;
                            pathCorrector(createPath, fullCreatePath);
                            if(checkPathValidity(fullCreatePath))
                            {
                                fullCreatePath+= "/"+fileName;
                                //cout<<"\nfullCreatePath : "<<fullCreatePath;
                                createFile(fullCreatePath);
                            } 
                            else{
                                cout<<"\nInvalid Path\n";
                            }
                        }
                    }

                    else{
                        cout<<"\nInvalid Create File Command";
                    }
                    
                }
            }

        }

        else if(!command_type.compare("create_dir"))
        {
            // Function for creating single directory
            // create_dir dir_name destination_path  -- Ex:- create_dir newFolder home/aman/Desktop
            // destination path 3 type ke hai handle kar raha hu
            // 1. agar current directory me create karna hai toh we can give relative path line dir1/destPath or ./dir1/destPath
            //            ## create_dir newFolder folder1/
            // 2. agar koi or directory me copy karna hai give full path 
            //            ## copy a1.txt /home/aman/destpath
            // 3. copy a1.txt ~/destpath   - Handling ~ case also
            string DirName = userCommand[1];
            string createPath = userCommand[2];
            string pathToCreate;
            if(userCommand.size()!=3)
            {
                cout<<"\nInvalid Create File Command";
                userCommand.clear();
            }
            else
            {
                if(createPath[0]=='.')
                {
                    vectorToString(curr_path, pathToCreate);
                    createPath = pathToCreate + createPath.substr(1);
                    if(checkPathValidity(createPath))
                    {
                        createPath+="/";
                        createPath+=DirName;
                        createDirectory(createPath);
                    } 
                    else{
                        cout<<"\nInvalid Path\n";
                    }
                }
                else{
                    string fullCreatePath;
                    pathCorrector(createPath, fullCreatePath);
                    if(checkPathValidity(fullCreatePath))
                    {
                        fullCreatePath+= "/"+DirName;
                        //cout<<"\nfullCreatePath : "<<fullCreatePath;
                        createDirectory(fullCreatePath);
                    } 
                    else{
                        cout<<"\nInvalid Path\n";
                    }
                }
            }
        }

        else if(!command_type.compare("copy"))
        {
            // Function for both copying files and directories - NOTE For copy file - can also copy multiple files at a time
            // Input : copy file1.txt destnationPath
            // destination path 3 type ke hai handle kar raha hu
            // 1. agar current directory me copy karna hai toh we can give relative path line dir1/destPath
            //            ## copy a1.txt folder1/
            // 2. agar koi or directory me copy karna hai give full path 
            //            ## copy a1.txt /home/aman/destpath
            // 3. copy a1.txt ~/destpath   - Handling ~ case also
            if(userCommand.size()<3)
            {
                // if command array is less than 3 chars then it means it is Invalid command
                // means i.e either source or directory is not present
                cout<<"\nInvalid Command";
                userCommand.clear();
            }
            else{
                string sourceFileOrDIR = userCommand[1];
                string reldestinationPath = userCommand[2];
                string destinationPath;
                pathCorrector(reldestinationPath, destinationPath);
                string sourcePath;
                char * pathCharPtr;
                stringToCharArr(sourceFileOrDIR, &pathCharPtr);
                struct stat file_info;
                stat(pathCharPtr, &file_info);
                //cout<<"\ndestinationPath :"<<destinationPath;
                if(S_ISDIR(file_info.st_mode))
                {
                    // Dir Logic
                    vectorToString(curr_path, sourcePath);
                    sourcePath = sourcePath + "/" + sourceFileOrDIR;  // sourceFileOrDIR refers to DirName
                    destinationPath = destinationPath + "/" + sourceFileOrDIR;
                    createDirectoryFromSourceWithPermission(destinationPath, sourcePath);
                    vector<string> path_content;
                    copyDirectoryContentFunction(sourcePath, destinationPath);
                }
                else{
                    // Copy File Logic
                    if(userCommand.size()>3)   // for copying multiple files
                    {
                        // copy f1.txt f2.txt f3.txt /home/aman/destinationFolder
                        sourceFileOrDIR = userCommand[1];
                        reldestinationPath = userCommand.back();
                        string destinationPath;
                        pathCorrector(reldestinationPath, destinationPath);
                        userCommand.pop_back();
                        for(int i=1;i<userCommand.size();i++)
                        {
                            sourceFileOrDIR = userCommand[i];
                            // cout<<"Value of i: "<<i<<" sourceFileOrDIR: "<<sourceFileOrDIR;
                            vectorToString(curr_path, sourcePath);
                            sourcePath = sourcePath + "/" + sourceFileOrDIR;
                            string destinationPath1 = destinationPath + "/" + sourceFileOrDIR; // sourceFileOrDIR refers to FileName
                            //cout<<"\nFile DEST PAth : "<<destinationPath;
                            createFile(destinationPath1);
                            copyDataFunction(sourcePath, destinationPath1);
                        }
                        // added to show goto directory content
                        content_info_refresh=true;  // making it true to update and print the new directory content
                        NormalModeSetting();
                        content_info_refresh=false;  // making it false again to not update the content
                    }
                    else{  // For copy single file
                        // copy f1.txt /home/aman/destinationFolder
                        //cout<<"\n Inside File ELSE";
                        vectorToString(curr_path, sourcePath);
                        sourcePath = sourcePath + "/" + sourceFileOrDIR;
                        destinationPath = destinationPath + "/" + sourceFileOrDIR; // // sourceFileOrDIR refers to FileName
                        createFile(destinationPath);
                        copyDataFunction(sourcePath, destinationPath);
                    }
                }
            }

        }

        else if(!command_type.compare("goto"))
        {   
            // like cd command
            // syntax goto DirName
            if(userCommand.size()==2)
            {
                string goTopath = userCommand[1];
                string fullgoTopath;
                pathCorrector(goTopath, fullgoTopath);
                backward_history.push(curr_path);
                gotoFunctionality(goTopath, currentPath, fullgoTopath);
            }
            else{
                cout<<"\nInvalid Command\n";
                userCommand.clear();
            }
        }

        else if(!command_type.compare("rename"))
        {
            // for renaming the directory or file
            // syntax :- rename oldName newName
            // rename a1.txt a4.txt  -- for files
            // rename dir1 dir2  -> rename dir1 to dir2
            if(userCommand.size()==3)
            {
                string currFileName = userCommand[1];
                string updateFileName = userCommand[2];
                contentRenameFunctionality(currFileName, updateFileName, currentPath);  // Rename the file
            }
            else{
                cout<<"\nInvalid Command\n";
                userCommand.clear();
            }
        }

        else if(!command_type.compare("delete_file"))
        {
            // delete the file
            // syntax :- delete_file fileName
            // ex: delete_file file1
            if(userCommand.size()==2)
            {
                string deleteFileName = userCommand[1];
                string fulldeleteFileName;
                pathCorrector(deleteFileName, fulldeleteFileName);
                removeFileFunctionality(fulldeleteFileName);
            }
            else{
                cout<<"\nInvalid Command\n";
                userCommand.clear();
            }

        }

        else if(!command_type.compare("delete_dir"))
        {
            // delete the directory
            // syntax :- delete_dir dirName
            // ex: delete_dir Folder1
            if(userCommand.size()==2)
            {
                string deleteDirName = userCommand[1];
                string fulldeleteDirName;
                pathCorrector(deleteDirName, fulldeleteDirName);
                removeDirectoryFunctionality(fulldeleteDirName);
            }
            else{
                cout<<"\nInvalid Command\n";
                userCommand.clear();
            }
        }

        else if(!command_type.compare("move"))
        {
            // move the directory or file
            // syntax:- move file/dirName destinationPath
            // move a1.txt /home/aman/dumps
            if(userCommand.size()==3)
            {
                string destContentName = userCommand.back();
                string destContentFullName;
                pathCorrector(destContentName, destContentFullName);            
                moveContentFunctionality(userCommand, destContentFullName, currentPath);
            }
            else{
                cout<<"\nInvalid Command\n";
                userCommand.clear();
            }
        }

        else if(!command_type.compare("quit"))
        {
            // Press quit to exit the program
            
            quit=false;
        }

        else if(user_input==27)
        {
            content_info_refresh=true;
            normal_file_mode=true;

            while(!forward_history.empty()){
                forward_history.pop();
            }
        }

        else{
            cout<<"\nWrong Command";
        }
    }
            

}

void keyPressForNormalMode(){
    int total_record = currPathContent.size();

    struct winsize curr_win_size;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &curr_win_size);   // use for terminal window size
    int max_record_count = curr_win_size.ws_row;
    max_record_count--;

    createNormalModeScreen=true;
    content_info_refresh=true;

    char character_input;
    cin.get(character_input);
    //cout<<"\nchar seq: "<<character_input;
    if(character_input == '\x1b'){ // Escape Key HexaDecimalCode - \x1b & ASCII code - 27

        // for storing - [C, [A, [B, [D
        char openbrac;  // [ - ASCII CODE - 91
        char cursorKey; //A,B,C,D - ASCII CODE - 65,66,67,68
        cin.get(openbrac);
        cin.get(cursorKey);
        if(cursorKey== 'A')  // esc_seq[1]== 'A'
        {   // Up Arrow Key                
            UpArrowKeyFunction();     
            content_info_refresh=false;  // hame directory content fir se fetch nahi karna hai
        }
        
        else if(cursorKey == 'B')
        {   // Down Arrow Key
            DownArrowKeyFunction(max_record_count, total_record);
            content_info_refresh=false;  // hame directory content fir se fetch nahi karna hai
        }
            
        else if(cursorKey == 'C')
        {   // Right Arrow Key
            RightArrowKeyFunction();   // Implementing Right Arrow Key Functionality
        }

        else if(cursorKey == 'D')
        {   // Left Arrow Key
            LeftArrowKeyFunction();   // Implementing Left Arrow Key Functionality
        }

        else{
            createNormalModeScreen=false;
            content_info_refresh=false;
        }

    }
    else{
        switch(character_input)
        {
            case 'q':   // Make Quit flag= true - to stop the program
                quit = false;  
                break;

            case 13:   // When user Press Enter key
                EnterKeyFunction();
                break;

            case 'h':  // For Home Screen - brings the cursor back to home screen where our program starts/resides
                HomekeyFunction();
                break;

            case 127:  // BackSapce Key - to go one level UP
                BackSpaceKeyFunction();
                break;

            case ':':
                normal_file_mode=false;
                cout<<"\x1b[999;1H";
                break;
            
            default:   // default case-> we do nothing
                createNormalModeScreen=false;
                content_info_refresh=false;
                break;
        }
    }
}


int main(){
    enableNonCanonicalMode();  // enable non canonical mode - means user cant't type anything and also disabling ctrl+* commands
    char runningDirectory[4096];
    getcwd(runningDirectory, 4096); 
    runningDirPath = runningDirectory; // capturing the running directory
    stringToVector(runningDirPath,curr_path);

    signal(SIGWINCH, checkForResize);   // capture the event of a terminal resize

    /* NOTE: In TERMINAL MODE: Press q to exit the program
             In COMMAND MODE: Type quit to exit the program
             Transition from TERMIANL MODE TO COMMAND MODE using ":"
             Transition from COMMAND MODE TO TERMIANL MODE using "ESC"  */

    while(quit){
        if(normal_file_mode){
            if(createNormalModeScreen){
                NormalModeSetting();
            }
            keyPressForNormalMode();
        }
        else{
            CommandModeFunction();
        }
    }

    // Clearing the screen before exiting the program
    cout<<"\x1b[H";
    cout<<"\x1b[2J";  // clear screen and send cursor to home position.
    cout<<"\x1b[H";  // send cursor to cursor home position.

    return 0;
}