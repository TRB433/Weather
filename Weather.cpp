#include <iostream>
#include <assert.h>
#include <sstream>
#include <fstream>
#include <curl/curl.h>
#include <SFML/Graphics.hpp>


using namespace std;

using namespace sf;

//used to check what should be rendered
enum MODE{
    INTRO, MAIN, EDIT
};

//x and y dimensions (integer)
struct Dim2Di
{
    int x,y;
}; 

//constant screen resolution
namespace GC{
    const Dim2Di SCREEN_RES{400,250};
}
    
size_t writeData(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

//Function to return specific data from curl HTTP request
//The data that is returned from request
//The type of data. For example, temp_c
string getData(string dataSet, string type){
    bool found = false;
    stringstream ss;

    for(int i = 0; i < dataSet.length(); i++){
        //checking if the word at the current position is the same as the type
        if(dataSet[i] == type[0] && dataSet[i + type.length() - 1] == type[type.length() - 1] && dataSet[i + 2] == type[2]){
            //all characters between i and i+j will be the type
            for(int j = 0; j < type.length(); j++){        
                //get all characters and check if it is the same as type        
                ss << dataSet[i + j];
                if(ss.str() == type){
                    found = true;
                }
            }

            //if the desired type has been found
            if(found){
                ss.str("");
                //offset used to iterate through data being held at type, start at 2 since theres brackets
                int offset = 2;
                //iterate through characters until the closing tag is found
                while(dataSet[i + type.length() - 1 + offset] != '<'){
                    ss << dataSet[i + type.length() - 1 + offset];
                    offset++;
                }
                return ss.str();
            }
        }
    }

    return "ERROR: TYPE NOT FOUND IN DATA SET";
}

//Allows you to write the result from HTTP request into something such as a string
size_t CurlWrite_CallbackFunc_StdString(void *contents, size_t size, size_t nmemb, std::string *s)
{
    size_t newLength = size*nmemb;
    try
    {
        s->append((char*)contents, newLength);
    }
    catch(std::bad_alloc &e)
    {
        //handle memory problem
        return 0;
    }
    return newLength;
}

//Window for changing location, saves new location to file.
void ChangeLocation(RenderWindow& window, Font font, Sprite bg, MODE &currentMode){

    void RenderWeather(RenderWindow& window, string location, Font font, Sprite bg, MODE &currentMode);

    //string to hold input from user
    string input;

    bool first = true;

    //Text to render on window
    Text inputText;
    Text mainText;
    inputText.setFont(font);
    mainText.setFont(font);
    mainText.setString("Enter your new city/region:");
    mainText.setFillColor(Color::White);
    mainText.setPosition(0,0);

    while(window.isOpen()){
        Event event;
        while(window.pollEvent(event)){        
            //if user enters text    
            if (event.type == sf::Event::TextEntered){       
                //if the text isnt the enter or backspace button           
                if(static_cast<char>(event.text.unicode) != '' && static_cast<char>(event.text.unicode) != ''){
                    input += static_cast<char>(event.text.unicode); //convert unicode to a char and add it to input            
                    inputText.setString(input); //set the input text being rendered to the current input
                    inputText.setPosition(0, GC::SCREEN_RES.y / 2); 
                }                                                                                      
            }

            //if key is pressed
            if(event.type == Event::KeyPressed){
                //if enter key is pressed
                if(event.key.code == Keyboard::Enter){
                    //user is finished changing settings, switch mode to main
                    currentMode = MODE::MAIN;
                }
                //if backspace is pressed
                if(event.key.code == Keyboard::Backspace){
                    //if the current input isnt empty
                    if(input != ""){
                        //remove front character from string and update text
                        input.pop_back();
                        inputText.setString(input);
                        inputText.setPosition(0, GC::SCREEN_RES.y / 2);  
                    }
                    
                }
            }

            switch(currentMode){
                //if the mode has been switched to main
                case MODE::MAIN:
                    //find and truncate the location file with the input
                    ofstream newFile;
                    newFile.open("location.txt", std::ofstream::out | std::ofstream::trunc);
                    newFile << input;
                    newFile.close();

                    //render main window again and close the current one
                    RenderWeather(window, input, font, bg, currentMode);
                    window.close();                                                       
                    
            }

            if(first){
                input = "";
                first = false;
            }
        }

        window.draw(bg);
        window.draw(inputText);
        window.draw(mainText);
        window.display();
        window.clear();
    }
}

//Main window, when opened HTTP request is made and results are displayed, if the request is invalid, change location is called
void RenderWeather(RenderWindow& window, string location, Font font, Sprite bg, MODE &currentMode){       

    string fileName = "icon.png"; 

    //curl handle, for making request and saving it in a string
    CURL *curl;

    //handle for downloading the icon from url
    CURL *curl1;

    //code returned from request
    CURLcode res;

    string s;

    //stores the url for the icon that is retrieved in the first request
    string iconString;

    //setting up Text
    Text instructions;
    Text suggestion;
    Text temp;
    Text rain;
    Text time;
    Text title;
    Sprite icon;
    Texture tex;
    instructions.setFont(font);
    suggestion.setFont(font);
    rain.setFont(font);
    temp.setFont(font);
    time.setFont(font);
    title.setFont(font);
    suggestion.setFillColor(Color::White);
    rain.setFillColor(Color::White);
    time.setFillColor(Color::White);
    title.setFillColor(Color::White);
    temp.setFillColor(Color::White);    
    title.setCharacterSize(40);
    rain.setCharacterSize(25);
    time.setCharacterSize(25);
    temp.setCharacterSize(25);
    instructions.setCharacterSize(15);
    instructions.setString("ESC -\nChange Location");
    instructions.setPosition(GC::SCREEN_RES.x, GC::SCREEN_RES.y - instructions.getGlobalBounds().height - 10);
    suggestion.setCharacterSize(20);
    title.setString(location);    
    title.setPosition(0 - title.getGlobalBounds().width, 0);

    //initialize curl
    curl_global_init(CURL_GLOBAL_DEFAULT);    
    curl  = curl_easy_init();
    curl1 = curl_easy_init();

    if(curl){     
        //open icon file
        FILE* fp = fopen(fileName.c_str(), "wb");   
        
        //HTTP request, adding the location
        string request = "http://api.weatherapi.com/v1/current.xml?key=884ca688c44e46d0b9d45619240609&q=" + location;

        //CURL OPTIONS
        //Set the URL to the request string
        curl_easy_setopt(curl, CURLOPT_URL, request.c_str());
        //Get the result as a string
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWrite_CallbackFunc_StdString);
        //Store the result inside s
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);

        //perform the request and store the response in res
        res = curl_easy_perform(curl);

        //if the response isnt ok, the location is invalid, so switch the mode to change location
        if(res != CURLE_OK){
            currentMode = MODE::EDIT;
            ChangeLocation(window,font,bg,currentMode);
            
        }            
        else
        {   //if the temp cant be found, the location cant be used, so switch the mode to change location    
            if(getData(s, "temp_c") == "ERROR: TYPE NOT FOUND IN DATA SET"){
                currentMode = MODE::EDIT;
                ChangeLocation(window,font,bg,currentMode);
            }    
            //getting the url for the icon          
            iconString = getData(s, "icon");
            iconString.erase(0,2);
            //get all the data to be displayed and store them in their strings
            temp.setString(getData(s, "temp_c") + " Degrees");  
            time.setString(getData(s, "text"));   
            rain.setString(getData(s, "precip_mm") + "mm of rainfall");                     
            temp.setPosition(0 - temp.getGlobalBounds().width, GC::SCREEN_RES.y/3.5f);             
            time.setPosition(0 - time.getGlobalBounds().width, temp.getPosition().y + temp.getGlobalBounds().height + 10);  
            rain.setPosition(0 - rain.getGlobalBounds(). width, time.getPosition().y + time.getGlobalBounds().height + 10);                
        }

        //For the second request, we're getting the icon from the url
        //Set the url to the iconstring that is storing the url from the first request
        curl_easy_setopt(curl1, CURLOPT_URL, iconString.c_str());
        //Setting the writefunction option to the writeData function, so data that is returned is written to storage
        curl_easy_setopt(curl1, CURLOPT_WRITEFUNCTION, writeData);
        //Setting the file to write the data to (fp)
        curl_easy_setopt(curl1, CURLOPT_WRITEDATA, fp);

        //Sending the request and storing the result code in res1
        CURLcode res1 = curl_easy_perform(curl1);
        curl_easy_cleanup(curl1);
        //Close the file
        fclose(fp);
        //output error message if icon wasnt downloaded
        if (res1 != CURLE_OK) 
            std::cout << "Failed to download image: " << curl_easy_strerror(res1) << std::endl;       
          
        curl_easy_cleanup(curl);
        curl_easy_cleanup(curl1);
    }

    //setting suggestions based on temperature/rainfall
    string tempString = temp.getString();
    string rainString = rain.getString();
    float rainFloat = stof(rainString);
    float tempFloat = stof(tempString);
    if(tempFloat < 15.0f){
        suggestion.setString("It's a little chilly,\nyou should wear a jacket");
    }
    if(tempFloat >= 15 && tempFloat < 24){
        suggestion.setString("It's fairly warm outside,\nno need for a jacket");
    }
    if(tempFloat >= 24.0f){
        suggestion.setString("Shorts and shirt weather today!");
    }
    if(rainFloat > 1.0f){
        suggestion.setString("It's raining a little,\nyou should wear a jacket");
    }
    if(rainFloat > 2.0f){
        suggestion.setString("It's raining quite a bit,\nyou should wear a coat");
    }
    

    suggestion.setPosition(0 - suggestion.getGlobalBounds().width, GC::SCREEN_RES.y - (suggestion.getGlobalBounds().height + 10));

    Sprite iconSprite;
    Texture iconTex;
    if(!iconTex.loadFromFile("icon.png")){
        assert(false);
    }

    iconSprite.setTexture(iconTex);
    iconSprite.setScale(1.5f,1.5f);

    iconSprite.setPosition(GC::SCREEN_RES.x, 0);

    while(window.isOpen()){                
        Event event;
        while (window.pollEvent(event)){
            if (event.type == sf::Event::Closed)
                window.close();

            //if escape is pressed
            if(event.type == Event::KeyPressed){
                if(event.key.code == Keyboard::Escape){
                    //switch mode to change location
                    currentMode = MODE::EDIT;
                }
            }

            switch(currentMode){
                //if mode has been switched to change location, change windows
                case MODE::EDIT:
                    ChangeLocation(window, font, bg, currentMode);
            }
        }

        //Slide UI into position
        if(suggestion.getPosition().x < 0){
            suggestion.setPosition(suggestion.getPosition().x + 0.25f, suggestion.getPosition().y);
        }
        if(iconSprite.getPosition().x > GC::SCREEN_RES.x - iconSprite.getGlobalBounds().width){
            iconSprite.setPosition(iconSprite.getPosition().x - 0.25f, iconSprite.getPosition().y);
        }
        if(title.getPosition().x < 0){
            title.setPosition(title.getPosition().x + 0.25f, title.getPosition().y);
        }
        if(rain.getPosition().x < 0){
            rain.setPosition(rain.getPosition().x + 0.25f, rain.getPosition().y);
        }
        if(time.getPosition().x < 0){
            time.setPosition(time.getPosition().x + 0.25f, time.getPosition().y);
        }
        if(temp.getPosition().x < 0){
            temp.setPosition(temp.getPosition().x + 0.25f, temp.getPosition().y);
        }
        if(instructions.getPosition().x > GC::SCREEN_RES.x - instructions.getGlobalBounds().width - 5){
            instructions.setPosition(instructions.getPosition().x - 0.25f, instructions.getPosition().y);
        }

               
        window.draw(bg);
        window.draw(instructions);
        window.draw(suggestion);
        window.draw(iconSprite);     
        window.draw(title); 
        window.draw(rain);     
        window.draw(time);            
        window.draw(temp);
        window.display();
        window.clear();
        
    }
}

//First time setup window, if location file already exists, it immediately calls renderweather
void RenderWelcome(RenderWindow& window, Text &welcomeText, Text &inputText, MODE &currentMode, Font font){

    //Creating background sprite and assigning texture
    Sprite bg;
    Texture tex;
    if(!tex.loadFromFile("data/blue.jpg")){
        assert(false);
    }
    bg.setTexture(tex);
    bg.setScale(5,5);
    bg.setPosition(GC::SCREEN_RES.x / 2 - bg.getGlobalBounds().width / 2, GC::SCREEN_RES.y / 2 - bg.getGlobalBounds().height / 2);

    string input;    

    //open location file
    fstream file;
    file.open("location.txt");
    //if the file doesnt exist, create a new one
    if(!file){              
        fstream newFile("location.txt", fstream::out);        
        newFile.close();      
    }
    else{
        //if the file does exist, store the contents of file in input, then render weather
        getline(file, input);
        RenderWeather(window, input, font, bg, currentMode);
        
    }    
       

    //stringstream input;
    
    bool firstInput = true;
    
    

    while(window.isOpen()){     
        Event event;
        while (window.pollEvent(event))
        {            
            if (event.type == sf::Event::Closed)
                window.close();

            if(event.type == Event::KeyPressed){
                if(event.key.code == Keyboard::Enter){
                    currentMode = MODE::MAIN;
                }

                if(event.key.code == Keyboard::Backspace){
                    if(input != ""){
                        input.pop_back();
                        inputText.setString(input);
                        inputText.setPosition(0, GC::SCREEN_RES.y / 2);  
                    }
                    
                }
            }

            switch(currentMode){
                //if the mode has been changed to main, store the current input in location file
                case MODE::MAIN:
                    ofstream newFile;
                    newFile.open("location.txt");
                    newFile << input;
                    newFile.close();
                    RenderWeather(window, input, font, bg, currentMode);
                    window.close();                                                       
                    
            }
            //stores user entered text in input, then text on screen is updated
            if (event.type == sf::Event::TextEntered){   
                if(static_cast<char>(event.text.unicode) != ''){
                    input += static_cast<char>(event.text.unicode);                
                    inputText.setString(input);
                    inputText.setPosition(0, GC::SCREEN_RES.y / 2);   
                }          
                                                                 
            }
        }        
        window.draw(bg);
        window.draw(inputText);
        window.draw(welcomeText);
        window.display();
        window.clear();
    }
}

int main(){
    //immediately set mode to intro
    MODE currentMode = MODE::INTRO;

    //creating window, using GC::SCREEN_RES to define the size
    RenderWindow window(VideoMode(GC::SCREEN_RES.x,GC::SCREEN_RES.y), "Weather");

    //setting up UI elements
    Text welcomeText;
    Text inputText;
    Font font;

    if(!font.loadFromFile("data/Roboto-Light.ttf")){
        assert(false);
    }

    inputText.setFont(font);
    welcomeText.setFont(font);
    welcomeText.setString("First time setup\nEnter your city/region:");
    inputText.setFillColor(Color::White);
    welcomeText.setFillColor(Color::White);
    welcomeText.setPosition(0, 0);    

    while(window.isOpen()){
        Event event;
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
                window.close();
        }

        switch(currentMode){
            case MODE::INTRO:
                RenderWelcome(window, welcomeText, inputText, currentMode, font);
        }
    }       

    

    //cout << "hello world";

    
}