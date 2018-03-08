


#include "lz/GTP.h"
#include <fstream>
#include <cassert>
#include <cstring>
#include <sstream>
#include <csignal>
#include <iostream>

using namespace std;

#ifndef NO_GUI_SUPPORT
#include "gui/ui.h"
#endif


static int opt_size = 19;
static bool opt_advisor = false;
static bool opt_selfplay = false;


int gtp(const string& cmdline, const string& selfpath);
int advisor(const string& cmdline, const string& selfpath);
int selfplay(const string& selfpath, const vector<string>& players);

int main(int argc, char **argv) {

    GTP::setup_default_parameters();
    
    string selfpath = argv[0];
    
    auto pos  = selfpath.rfind(
        #ifdef _WIN32
        '\\'
        #else
        '/'
        #endif
        );

    selfpath = selfpath.substr(0, pos); 

    vector<string> players;

    for (int i=1; i<argc; i++) {
        string opt = argv[i];

        if (opt == "--pass") {
            i++;
            string append;
            for (; i<argc; i++) {
                append += " ";
                append += argv[i];
            }
            for (auto& line : players) {
                line += append;
            }
            break;
        }

        if (opt == "--advisor") {
            opt_advisor = true;
        }
        else if (opt == "--selfplay") {
            opt_selfplay = true;
        }
        else if (opt == "--player") {
            string player = argv[++i];
            if (player.find(" ") == string::npos && player.find(".txt") != string::npos)
                player = "./leelaz -g -w " + player;
            players.push_back(player);
        }
        else if (opt == "--size") {
            opt_size = stoi(argv[++i]);
        }
        else if (opt == "--threads" || opt == "-t") {
                int num_threads = std::stoi(argv[++i]);
                if (num_threads > cfg_num_threads) {
                    fprintf(stderr, "Clamping threads to maximum = %d\n", cfg_num_threads);
                } else if (num_threads != cfg_num_threads) {
                    fprintf(stderr, "Using %d thread(s).\n", num_threads);
                    cfg_num_threads = num_threads;
                }
        } 
        else if (opt == "--playouts" || opt == "-p") {
                cfg_max_playouts = std::stoi(argv[++i]);
        }
            else if (opt == "--noponder") {
                cfg_allow_pondering = false;
            }
            else if (opt == "--visits" || opt == "-v") {
                cfg_max_visits = std::stoi(argv[++i]);
            }
            else if (opt == "--lagbuffer" || opt == "-b") {
                int lagbuffer = std::stoi(argv[++i]);
                if (lagbuffer != cfg_lagbuffer_cs) {
                    fprintf(stderr, "Using per-move time margin of %.2fs.\n", lagbuffer/100.0f);
                    cfg_lagbuffer_cs = lagbuffer;
                }
            }
            else if (opt == "--resignpct" || opt == "-r") {
                cfg_resignpct = std::stoi(argv[++i]);
            }
            else if (opt == "--seed" || opt == "-s") {
                cfg_rng_seed = std::stoull(argv[++i]);
                if (cfg_num_threads > 1) {
                    fprintf(stderr, "Seed specified but multiple threads enabled.\n");
                    fprintf(stderr, "Games will likely not be reproducible.\n");
                }
            }
            else if (opt == "--dumbpass" || opt == "-d") {
                cfg_dumbpass = true;
            }
            else if (opt == "--weights" || opt == "-w") {
                cfg_weightsfile = argv[++i];
            }
            else if (opt == "--logfile" || opt == "-l") {
                cfg_logfile = argv[++i];
                fprintf(stderr, "Logging to %s.\n", cfg_logfile.c_str());
                cfg_logfile_handle = fopen(cfg_logfile.c_str(), "a");
            }
            else if (opt == "--quiet" || opt == "-q") {
                cfg_quiet = true;
            }
            #ifdef USE_OPENCL
            else if (opt == "--gpu") {
                cfg_gpus = {std::stoi(argv[++i])};
            }
            #endif
            else if (opt == "--puct") {
                cfg_puct = std::stof(argv[++i]);
            }
            else if (opt == "--softmax_temp") {
                cfg_softmax_temp = std::stof(argv[++i]);
            }
        else if (opt == "--fpu_reduction") {
            cfg_fpu_reduction = std::stof(argv[++i]);
        }
        else if (opt == "--timemanage") {
            std::string tm = argv[++i];
            if (tm == "auto") {
                cfg_timemanage = TimeManagement::AUTO;
            } else if (tm == "on") {
                cfg_timemanage = TimeManagement::ON;
            } else if (tm == "off") {
                cfg_timemanage = TimeManagement::OFF;
            } else {
                fprintf(stderr, "Invalid timemanage value.\n");
                throw std::runtime_error("Invalid timemanage value.");
            }
        }
    }

    if (cfg_timemanage == TimeManagement::AUTO) {
        cfg_timemanage = TimeManagement::ON;
    }

    if (cfg_max_playouts < std::numeric_limits<decltype(cfg_max_playouts)>::max() && cfg_allow_pondering) {
        fprintf(stderr, "Nonsensical options: Playouts are restricted but "
                            "thinking on the opponent's time is still allowed. "
                            "Ponder disabled.\n");
        cfg_allow_pondering = false;
    }
    
    if (cfg_weightsfile.empty() && players.empty()) {
        fprintf(stderr, "A network weights file is required to use the program.\n");
        throw std::runtime_error("A network weights file is required to use the program");
    }

    fprintf(stderr, "RNG seed: %llu\n", cfg_rng_seed);

    if (players.size() > 1 || opt_selfplay)
        selfplay(selfpath, players);
    else if (opt_advisor)
        advisor(players.empty() ? "" : players[0], selfpath);
    else
        gtp(players.empty() ? "" : players[0], selfpath);

    return 0;
}

int gtp(const string& cmdline, const string& selfpath) {

    GtpChoice agent;

#ifndef NO_GUI_SUPPORT
    go_window my_window;
#endif


    agent.onOutput = [&](const string& line) {
#ifndef NO_GUI_SUPPORT
        my_window.update(agent.get_move_sequence());
#endif
        cout << line;
    };

    if (cmdline.empty())
        agent.execute();
    else
        agent.execute(cmdline, selfpath);

    // User player input
    std::thread([&]() {

		while(true) {
			std::string input_str;
			getline(std::cin, input_str);
            agent.send_command(input_str);
            if (input_str == "quit")
                break;
		}

	}).detach();

#ifndef NO_GUI_SUPPORT
    my_window.wait_until_closed();
#endif

    agent.join();
    return 0;
}

int advisor(const string& cmdline, const string& selfpath) {

    GameAdvisor<GtpChoice> agent;
    //GameAdvisor agent("/home/steve/dev/app/leelaz -w /home/steve/dev/data/weights.txt -g");
    
#ifndef NO_GUI_SUPPORT
    go_window my_window;
#endif

    agent.onGtpIn = [](const string& line) {
        cout << line << endl;
    };

    agent.onGtpOut = [&](const string& line) {
#ifndef NO_GUI_SUPPORT
        my_window.update(agent.get_move_sequence());
#endif
        cout << line;
    };

    agent.onThinkMove = [&](bool black, int move) {

        agent.place(black, move);
    };

    if (cmdline.empty())
        agent.execute();
    else
        agent.execute(cmdline, selfpath);

    // User player input
    std::thread([&]() {

		while(true) {
			std::string input_str;
			getline(std::cin, input_str);

            auto pos = agent.text_to_move(input_str);
            if (pos != GtpState::invalid_move) {
                agent.place(agent.next_move_is_black(), pos);
            }
            else
                agent.send_command(input_str);

            if (input_str == "quit")
                break;
		}

	}).detach();

    // computer play as BLACK
    agent.reset(true);

    while (true) {
        this_thread::sleep_for(chrono::microseconds(100));
        agent.pop_events();
        if (!agent.alive())
            break;
    }

#ifndef NO_GUI_SUPPORT
    my_window.wait_until_closed();
#endif

    agent.join();
    return 0;
}

int selfplay(const string& selfpath, const vector<string>& players) {

    GtpChoice black;
    GtpChoice white;

    black.onInput = [](const string& line) {
        cout << line << endl;;
    };

    black.onOutput = [](const string& line) {
        cout << line;
    };

    GtpChoice* white_ptr = nullptr;

    if (players.empty())
        black.execute();
    else
        black.execute(players[0], selfpath, 15);

    if (!black.isReady()) {
        std::cerr << "cannot start player 1" << std::endl;
        return -1;
    }

    if (players.size() > 1) {
        white_ptr = &white;
        white.execute(players[1], selfpath, 15);
        if (!white.isReady()) {
            std::cerr << "cannot start player 2" << std::endl;
            return -1;
        }
    }

#ifndef NO_GUI_SUPPORT
    go_window my_window;
#endif

    bool ok;

    if (opt_size != 19) {
        GtpState::send_command_sync(black, "boardsize " + to_string(opt_size), ok);
        if (!ok) {
            std::cerr << "player 1 do not support size " << opt_size << std::endl;
            return -1;
        }

        if (white_ptr) {
            GtpState::send_command_sync(*white_ptr, "boardsize " + to_string(opt_size), ok);
            if (!ok) {
                std::cerr << "player 2 do not support size " << opt_size << std::endl;
                return -1;
            }
        }
    }

    auto me = &black;
    auto other = white_ptr;
    bool black_to_move = true;
    bool last_is_pass = false;
    string result;

    
    GtpState::send_command_sync(black, "clear_board");
    if (white_ptr)
        GtpState::send_command_sync(*white_ptr, "clear_board");

#ifndef NO_GUI_SUPPORT
    my_window.reset(black.boardsize());
#endif

    for (int move_count = 0; move_count<361*2; move_count++) {
            
        auto vtx = GtpState::send_command_sync(*me, black_to_move ? "genmove b" : "genmove w", ok);
        if (!ok)
            throw runtime_error("unexpect error while genmove");

        if (vtx == "resign") {
            result = black_to_move ? "W+Resign" : "B+Resign";
            break;
        }

        if (vtx == "pass") {
            if (last_is_pass) 
                break;
            last_is_pass = true;
        } else
            last_is_pass = false;
            
#ifndef NO_GUI_SUPPORT
        auto move = me->text_to_move(vtx);
        my_window.update(black_to_move, move);
#endif

        if (other) {
            GtpState::send_command_sync(*other, (black_to_move ? "play b " : "play w ") + vtx, ok);
            if (!ok)
                throw runtime_error("unexpect error while play");

            auto tmp = me;
            me = other;
            other = tmp;
        }

        black_to_move = !black_to_move;
    }
 
        // a game is over 
    if (result.empty()) {
        result = GtpState::send_command_sync(black, "final_score", ok);
    }

    std::cout << "Result: " << result << std::endl;

#ifndef NO_GUI_SUPPORT
    my_window.wait_until_closed();
#endif

    GtpState::wait_quit(black);
    if (white_ptr) GtpState::wait_quit(*white_ptr);

    return 0;
}