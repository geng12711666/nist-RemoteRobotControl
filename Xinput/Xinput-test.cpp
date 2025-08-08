// XInput-testing.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <fstream>
#include <direct.h> // For _getcwd() on Windows
#include <limits.h> // For PATH_MAX or MAX_PATH


//#ifdef _WIN32
//#  ifdef USE_ASIO
//#      include <SDKDDKVer.h>   //     Set the proper SDK version before including boost/Asio 
//#      include <boost/asio.hpp>    //     Note boost/ASIO includes Windows.h.
//#   else //  USE_ASIO
//#      include <Windows.h>
//#   endif //  USE_ASIO
//#else // _WIN32
//#  ifdef USE_ASIO
//#     include <boost/asio.hpp>
//#  endif // USE_ASIO
//#endif //_WIN32

#include <boost/asio.hpp>   // current version : boost_1_84_0
#include <windows.h>
#include <xinput.h>
#include <chrono>
#include <cmath>
#include "common.h"
#include <thread>
//#include <string>

//using Clock = std::chrono::high_resolution_clock;     // This clock has higher resolution, but the ref epoch time is not guaranteed to be the Unix epoch (which is what python is using)
using Clock = std::chrono::system_clock;
using namespace std::chrono_literals;


#define MAX_USER_COUNT 1
#define DEADZONE_RATIO 0.1
#define QUANTIZE_LEVEL   1000
#define THUMB_MAX   32767.0
#define TRIGGER_MAX   255.0
#define ACCESS_PERIOD 20ms

#define LEFT_THUMB_DEADZONE  THUMB_MAX*DEADZONE_RATIO
#define RIGHT_THUMB_DEADZONE  THUMB_MAX*DEADZONE_RATIO

#define TRIGGER_DEADZONE  TRIGGER_MAX*DEADZONE_RATIO

// socket relate 
#define USE_JSON_STR (0)
using boost::asio::ip::udp;




/*
Some notes :
    TODO : currently rhe VelocityControllerMessage.cpp and common.h arelinked through solution. Need to make them a local copy in this project in the future
           need to put all the pre-requisites in the README file in the future

    - Before compiling :
        * The validated working boost library is boost_1_84_0
        * Need to add <windows.h> for xinput reference
        * Need to add <xinput.lib> to the linker  
        * Note that windows.h uses winsock.h and boost:assio uses winsock2. So if you need to include
          both library, you need to include boost::assio before windows.h, or follow the solution in
          https://stackoverflow.com/questions/9750344/boostasio-winsock-and-winsock-2-compatibility-issue

    - Controller state contains dwPacketNumber and the Gamepad info.
    - dwPacketNumber can be used to check whether the polling is too slow or too fast
    - The controller state will remains the same if no modification. E.g. If you press button A and then pad A will always be True.
    - cpp GUI : https://www.simplilearn.com/tutorials/cpp-tutorial/cpp-gui
    - Asynchronous timer : https://www.boost.org/doc/libs/1_83_0/doc/html/boost_asio/tutorial/tuttimer2.html
    - Using boost asynchronous timer : https://stackoverflow.com/questions/61334533/c-boost-asynchronous-timer-to-run-in-parallel-with-program
    - Use UDP socket from boost library : https://www.boost.org/doc/libs/1_81_0/doc/html/boost_asio/tutorial/tutdaytime4.html


*/

bool process_XInput(udp::socket* skt, udp::endpoint* rcv_end, std::ofstream& outputFile, bool& running_flag);

typedef bool (*callback_fp)(udp::socket* skt, udp::endpoint* rcv_end, std::ofstream& outputFile, bool& running_flag);

class timer_wrapper {

private:
    Clock::duration const interval;
    boost::asio::high_resolution_timer timer;
    callback_fp callback;
    udp::socket* udp_socket;
    udp::endpoint* receiver_endpoint;
    std::ofstream outputFile;              //  Creates or append to the event-timestamp.csv
    bool running_flag;

public:
    timer_wrapper(boost::asio::io_context& io, Clock::duration i, callback_fp fp, udp::socket *skt, udp::endpoint *rcv_end, std::string& filename)
        : interval(i), timer(io), callback(fp), udp_socket(skt), receiver_endpoint(rcv_end)
    {
        // Open or append to the event file
        outputFile.open(filename + "_cpp-event-timestamp.csv", std::ios::out | std::ios::app); // Creates or append to the event-timestamp.csv
        //outputFile.open("XXX_event-timestamp.csv", std::ios::out | std::ios::app); // Creates or append to the event-timestamp.csv

        // Write the headers to csv file
        outputFile << "timestamp, entity(0:controller 1:sensor), event(0:go home 1:start 2:detect),\n";

        // Set the running flag
        running_flag = false;
    }

    ~timer_wrapper() {
        if (outputFile.is_open()) {
            outputFile.close();   // Close the event timestamp csv file
        }
    }

    void run(){
        timer.expires_from_now(interval);
        timer.async_wait([=](boost::system::error_code ec) {
            if (!ec && callback(udp_socket, receiver_endpoint, outputFile, running_flag)) {
                run();
            }
            else {
                stop();
            }
            });
    }

    void stop() {
        timer.cancel();
    }

};







/// Start of Main function

int main(int argc, char* argv[])
{
    std::cout << "Hello! Program start\n";
    
    char buffer[MAX_PATH]; // Or MAX_PATH on Windows
    if (_getcwd(buffer, sizeof(buffer)) != NULL) {
        std::cout << "Current working directory: " << buffer << std::endl;
    }
    else {
        std::cerr << "Error getting current working directory" << std::endl;
    }


    // Start creating a UDP socket
    // program option variables
    std::string serverPort = "17998";
    std::string serverAddr = "10.10.0.162";   // IP for wired baseline
    //std::string serverAddr = "12.1.1.136";      // IP for 5G setup
    std::string out_filename = "default";
    
    
    // program options   
    namespace po = boost::program_options;
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "print the help message")
        ("port,p", po::value<std::string>(&serverPort), "server port number")
        ("addr,a", po::value<std::string>(&serverAddr), "bind address")
        ("outfile,o", po::value<std::string>(&out_filename), "output filename for event timestamp record")
        ;

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        if (vm.count("help"))
        {
            std::cout << desc << std::endl;
            return 0;
        }

        po::notify(vm);
    }
    catch (po::error& e) {
        std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
        std::cerr << desc << std::endl;
        return 0;
    }


    // Create udp socket with io_context
    boost::asio::io_context io_context;
    udp::resolver resolver(io_context);
    std::cout << "Server: " << serverAddr << " Port: " << serverPort << std::endl;
    udp::endpoint receiver_endpoint =
        *resolver.resolve(udp::v4(), serverAddr, serverPort).begin();
    udp::socket socket(io_context);
    socket.open(udp::v4());

    std::cout << "Trying to build udp connection with ip:" << serverAddr << " port:"<< serverPort << "\n\n";

    std::cout << "udp message is the Xinput signal from the controller in the form of : \n";
    std::cout << "udp:(v_horizontal) (v_vertical) (v_fwd) (thetadot_horizontal) (thetadot_vertical) (btnA) (btnB) (btnX) (btnY) \n\n";

    // Open and create a file for event timestamp record
    //std::ofstream outputFile;
    //outputFile.open(out_filename+"_event-timestamp.csv", std::ios::out | std::ios::app); // Creates or append to the event-timestamp.csv
    
    // Inquiry the controller for every ACCESS_PERIOD time and send the messages through the socket created
    boost::asio::io_context io_access_controller;
    timer_wrapper access_loop(io_access_controller, ACCESS_PERIOD, process_XInput, &socket, &receiver_endpoint, out_filename);
    //timer_wrapper access_loop(io_access_controller, ACCESS_PERIOD, process_XInput, &socket, &receiver_endpoint);
    access_loop.run();
    io_access_controller.run();


    // Close socket if needed
    

    return 0;
}




bool process_XInput(udp::socket* skt, udp::endpoint* rcv_end, std::ofstream& outputFile, bool& running_flag) {
    DWORD controller_idx = NULL;

    //auto loop_start = Clock::now();
    //auto loop_start = std::chrono::steady_clock::now();

    for (DWORD i = 0; i < MAX_USER_COUNT; i++)
    {
        XINPUT_STATE controller_state;
        ZeroMemory(&controller_state, sizeof(XINPUT_STATE));

        // Simply get the state of the controller from XInput.
        controller_idx = XInputGetState(i, &controller_state);

        if (controller_idx == ERROR_SUCCESS)
        {
            // Controller is connected
            //std::cout << "Controller " << i << " is connected!!\n"; //debug use
            XINPUT_GAMEPAD* pad_input = &controller_state.Gamepad;
            bool ctl_trig = false;
            int ctl_event = 0;     // check what event is for the Xinput signal. 0: not used, 1:move event, 2:go home

            //// Buttons control ////

            VelocityControllerMessage msg;
            msg.v_vertical = 0.0;
            msg.v_horizontal = 0.0;
            msg.v_fwd = 0.0;
            msg.thetadot_horizontal = 0.0;
            msg.thetadot_vertical = 0.0;
            

            // check whether the four buttons are pressed
            //std::cout << "Button mask : " << pad_input->wButtons << "\n"; //debug use
            msg.btnA = pad_input->wButtons & XINPUT_GAMEPAD_A;
            msg.btnB = pad_input->wButtons & XINPUT_GAMEPAD_B;
            msg.btnX = pad_input->wButtons & XINPUT_GAMEPAD_X;
            msg.btnY = pad_input->wButtons & XINPUT_GAMEPAD_Y;
            //bool pad_Lthumb = pad_input->wButtons & XINPUT_GAMEPAD_LEFT_THUMB;  //this is the thumb press button 
            //bool pad_Rthumb = pad_input->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB;

            if (msg.btnA) {     // Button A is for go home command
                std::cout << "Button A is pressed. \n";
                std::cout << "Robot is going to home position ... \n";
                ctl_trig = true;
                ctl_event = 2;
                //std::cout << "Packet number : " << controller_state.dwPacketNumber << "\n\n";;
            }
            else if (msg.btnB) {    //Currently not used
                std::cout << "Button B is pressed \n";
                ctl_trig = true;
                //std::cout << "Packet number : " << controller_state.dwPacketNumber << "\n\n";;
            }
            else if (msg.btnX) {    //Currently not used
                std::cout << "Button X is pressed \n";
                //std::cout << "Packet number : " << controller_state.dwPacketNumber << "\n\n";;
                ctl_trig = true;
            }
            else if (msg.btnY) {    // Button Y is for termination of program
                std::cout << "Button Y is pressed. Program terminates ... \n";
                return false;
            }


            //// Left Thumb stick control ////
            float Lthumb_X = pad_input->sThumbLX;
            float Lthumb_Y = pad_input->sThumbLY;

            // Check whether larger than deadzone
            if (abs(Lthumb_X) > LEFT_THUMB_DEADZONE) {
                // adjust magnitude relative to the thumbstick dead zone
                msg.v_horizontal = Lthumb_X/ THUMB_MAX;
                ctl_trig = true;
                ctl_event = 1;
            }
            if (abs(Lthumb_Y) > LEFT_THUMB_DEADZONE) {
                // adjust magnitude relative to the thumbstick dead zone
                msg.v_vertical = Lthumb_Y/ THUMB_MAX;
                ctl_trig = true;
                ctl_event = 1;
            }

            //// Right Thumb stick control ////
            float Rthumb_X = pad_input->sThumbRX;
            float Rthumb_Y = pad_input->sThumbRY;

            // Check whether larger than deadzone
            if (abs(Rthumb_X) > RIGHT_THUMB_DEADZONE) {
                // adjust magnitude relative to the thumbstick dead zone
                msg.thetadot_horizontal = Rthumb_X/ THUMB_MAX;
                ctl_trig = true;
                ctl_event = 1;
            }
            if (abs(Rthumb_Y) > RIGHT_THUMB_DEADZONE) {
                // adjust magnitude relative to the thumbstick dead zone
                msg.thetadot_vertical = Rthumb_Y/ THUMB_MAX;
                ctl_trig = true;
                ctl_event = 1;
            }

            
            //// Trigger control ////
            float Rtrigger = pad_input->bRightTrigger;
            float Ltrigger = pad_input->bLeftTrigger;
            msg.v_fwd = (Rtrigger - Ltrigger);
            if (abs(msg.v_fwd) > TRIGGER_DEADZONE) {
                // adjust magnitude relative to the Trigger dead zone
                msg.v_fwd = msg.v_fwd / TRIGGER_MAX;
                ctl_trig = true;
                ctl_event = 1;
            }


            //// Send to socket  ////
            // send only if there's input from the controller
            if (ctl_trig) {
                std::ostringstream ssout;
                ssout << msg;
                std::cout << "udp :" << ssout.str() << std::endl;
                std::string send_buf(ssout.str());
                skt->send_to(boost::asio::buffer(send_buf), *rcv_end);
                //std::cout << "Sent the message" << std::endl;
                
                if (ctl_event == 1) {    // Check if the robot control is a movement to trigger the event record
                    if (!running_flag) {    // check if the robot is running or not. If not, then mark the current time as robot start event

                        // Current time to seconds since epoch
                        auto epoch_s = std::chrono::duration_cast<std::chrono::milliseconds>(
                            Clock::now().time_since_epoch()
                        ).count();
                        outputFile << epoch_s << ", 0, 1,\n";
                        running_flag = true;
                    }
                }
                else if (ctl_event == 2) {  // Checck if go home is triggered.
                    if (running_flag) {    // check if the robot is running or not. If yes, then mark the current time as robot go home event
                        auto epoch_s = std::chrono::duration_cast<std::chrono::milliseconds>(
                            Clock::now().time_since_epoch()
                        ).count();
                        outputFile << epoch_s << ", 0, 0,\n";
                        std::this_thread::sleep_for(std::chrono::seconds(3)); // let this thread wait for 3 second for robot to go back to home position
                        // Current time to seconds since epoch
                        running_flag = false;
                    }
                }
                    
            }
            
        }
        else
        {
            // Controller is not connected
            std::cout << "Controller " << i << " is not connected!!\n";
        }
    }
    return true;
}
// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file







///////////////////////// Old Xinput processing code   //////////////////////////////////////
/*
// check whether the four buttons are pressed
            //std::cout << "Button mask : " << pad_input->wButtons << "\n"; //debug use
bool pad_A = pad_input->wButtons & XINPUT_GAMEPAD_A;
bool pad_B = pad_input->wButtons & XINPUT_GAMEPAD_B;
bool pad_X = pad_input->wButtons & XINPUT_GAMEPAD_X;
bool pad_Y = pad_input->wButtons & XINPUT_GAMEPAD_Y;
//bool pad_Lthumb = pad_input->wButtons & XINPUT_GAMEPAD_LEFT_THUMB;  //this is the thumb press button 
//bool pad_Rthumb = pad_input->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB;

if (pad_A) {
    std::cout << "Button A is pressed \n";
    //std::cout << "Packet number : " << controller_state.dwPacketNumber << "\n\n";;
}
else if (pad_B) {
    std::cout << "Button B is pressed \n";
    //std::cout << "Packet number : " << controller_state.dwPacketNumber << "\n\n";;
}
else if (pad_X) {
    std::cout << "Button X is pressed \n";
    //std::cout << "Packet number : " << controller_state.dwPacketNumber << "\n\n";;
}
else if (pad_Y) {
    return false;
}


//// Left Thumb stick control ////

float Lthumb_X = pad_input->sThumbLX;
float Lthumb_Y = pad_input->sThumbLY;

// Calculate the Magnitude
float raw_Lthumb_magnitude = sqrt(Lthumb_X * Lthumb_X + Lthumb_Y * Lthumb_Y);
float Lthumb_magnitude = raw_Lthumb_magnitude;
int quantize_Lthumb_X = 0, quantize_Lthumb_Y = 0;

// Check whether larger than deadzone
if (Lthumb_magnitude > LEFT_THUMB_DEADZONE) {

    //clip the magnitude to prevent from exceeding its expected max
    if (Lthumb_magnitude > THUMB_MAX) {
        Lthumb_magnitude = THUMB_MAX;
    }

    // adjust magnitude relative to the thumbstick dead zone
    Lthumb_magnitude = Lthumb_magnitude - LEFT_THUMB_DEADZONE;

    // quantize the value, not sure if needed
    Lthumb_magnitude = Lthumb_magnitude / (THUMB_MAX - LEFT_THUMB_DEADZONE) * QUANTIZE_LEVEL;
    quantize_Lthumb_X = Lthumb_magnitude * (Lthumb_X / raw_Lthumb_magnitude);
    quantize_Lthumb_Y = Lthumb_magnitude * (Lthumb_Y / raw_Lthumb_magnitude);

    //auto loop_end = Clock::now();
    //std::cout << "Processing time : " << std::chrono::duration_cast<std::chrono::microseconds>(loop_end - loop_start).count()
    //    << " microseconds\n";
        //std::cout << "Packet number : " << controller_state.dwPacketNumber << "\n";
        //std::cout << "Left thumbstick value : " << Lthumb_magnitude << "\n"
        //    << "Lx = " << quantize_Lthumb_X << ", Ly = " << quantize_Lthumb_Y
        //    << "\n\n";
}
else {
    Lthumb_magnitude = 0;
}

//// Left Thumb stick control ////

float Rthumb_X = pad_input->sThumbRX;
float Rthumb_Y = pad_input->sThumbRY;

// Calculate the Magnitude
float raw_Rthumb_magnitude = sqrt(Rthumb_X * Rthumb_X + Rthumb_Y * Rthumb_Y);
float Rthumb_magnitude = raw_Rthumb_magnitude;
int quantize_Rthumb_X = 0, quantize_Rthumb_Y = 0;

// Check whether larger than deadzone
if (Rthumb_magnitude > RIGHT_THUMB_DEADZONE) {

    //clip the magnitude to prevent from exceeding its expected max
    if (Rthumb_magnitude > THUMB_MAX) {
        Rthumb_magnitude = THUMB_MAX;
    }

    // adjust magnitude relative to the thumbstick dead zone
    Rthumb_magnitude = Rthumb_magnitude - RIGHT_THUMB_DEADZONE;

    // quantize the value, not sure if needed
    Rthumb_magnitude = Rthumb_magnitude / (THUMB_MAX - RIGHT_THUMB_DEADZONE) * QUANTIZE_LEVEL;
    int quantize_Rthumb_X = Rthumb_magnitude * (Rthumb_X / raw_Rthumb_magnitude);
    int quantize_Rthumb_Y = Rthumb_magnitude * (Rthumb_Y / raw_Rthumb_magnitude);

}
else {
    Rthumb_magnitude = 0;
}

// Calculate the processing time for each controller poll if any thumbsticks controller is used
if (Lthumb_magnitude || Rthumb_magnitude) {
    auto loop_end = Clock::now();
    std::cout << "-----------------------------------------------\n";
    std::cout << "Processing time : " << std::chrono::duration_cast<std::chrono::microseconds>(loop_end - loop_start).count()
        << " microseconds\n";
}

//Print the thumbsticks value if not 0
if (Lthumb_magnitude != 0) {
    std::cout << "Left thumbstick value : " << Lthumb_magnitude << "\n"
        << "Lx = " << quantize_Lthumb_X << ", Ly = " << quantize_Lthumb_Y
        << "\n\n";
}
if (Rthumb_magnitude != 0) {
    std::cout << "Right thumbstick value : " << Rthumb_magnitude << "\n"
        << "Lx = " << quantize_Rthumb_X << ", Ly = " << quantize_Rthumb_Y
        << "\n\n";
}

*/