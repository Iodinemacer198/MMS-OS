#include <stdint.h>
#include "wordle.h"

extern void print(const char* str);
extern void println(const char* str);
extern void printint(int num);
extern int atoi(const char* str);
extern char get_key();
extern bool isdigit(char c);
extern void putchar(char c);

bool contains(const char* buffer, int length, char target) {
    for (int i = 0; i < length; i++) {
        if (buffer[i] == target) {
            return true;
        }
    }

    return false;
}

extern int cursorX;
extern int cursorY;

#define W_BUFFER 128
char w_buffer[W_BUFFER];
int w_index = 0;

bool isletter(char c) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
        return true; 
    } else {
        return false; 
    }
}

int strlength(char* str) {
    int len = 0;
    while (str[len] != '\0')
        len++;
    return len;
}

static inline unsigned int get_cpu_cycles() {
    unsigned int lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
    
    return lo; 
}

static unsigned int rand_seed = 12345;

void os_srand(unsigned int seed) {
    rand_seed = seed;
}

unsigned int os_rand() {
    rand_seed = rand_seed * 1103515245 + 12345;
    return rand_seed;
}

const char* words[] = {
"apple","about","above","actor","acute","admit","adopt","adult","after","again",
"agent","agree","ahead","alarm","album","alert","alike","alive","allow","alone",
"along","alter","among","anger","angle","angry","apart","apple","apply","arena",
"argue","arise","array","aside","asset","audio","audit","avoid","award","aware",
"badly","baker","bases","basic","basis","beach","began","begin","begun","being",
"below","bench","billy","birth","black","blame","blind","block","blood","board",
"boost","booth","bound","brain","brand","bread","break","breed","brief","bring",
"broad","broke","brown","build","built","buyer","cable","calif","carry","catch",
"cause","chain","chair","chart","chase","cheap","check","chest","chief","child",
"china","chose","civil","claim","class","clean","clear","clerk","click","clock",
"close","coach","coast","could","count","court","cover","craft","crash","cream",
"crime","cross","crowd","crown","curve","cycle","daily","dance","dated","dealt",
"death","debut","delay","depth","doing","doubt","dozen","draft","drama","drawn",
"dream","dress","drill","drink","drive","drove","dying","eager","early","earth",
"eight","elite","empty","enemy","enjoy","enter","entry","equal","error","event",
"every","exact","exist","extra","faith","false","fault","fiber","field","fifth",
"fifty","fight","final","first","fixed","flash","fleet","floor","fluid","focus",
"force","forth","forty","forum","found","frame","frank","fraud","fresh","front",
"fruit","fully","funny","giant","given","glass","globe","going","grace","grade",
"grand","grant","grass","great","green","gross","group","grown","guard","guess",
"guest","guide","happy","harry","heart","heavy","hence","henry","horse","hotel",
"house","human","ideal","image","index","inner","input","issue","japan","jimmy",
"joint","jones","judge","known","label","large","laser","later","laugh","layer",
"learn","lease","least","leave","legal","level","lever","light","limit","links",
"lives","local","logic","loose","lower","lucky","lunch","lying","magic","major",
"maker","march","maria","match","maybe","mayor","meant","media","metal","might",
"minor","minus","mixed","model","money","month","moral","motor","mount","mouse",
"mouth","movie","music","needs","never","newly","night","noise","north","noted",
"novel","nurse","occur","ocean","offer","often","order","other","ought","paint",
"panel","paper","party","peace","peter","phase","phone","photo","piece","pilot",
"pitch","place","plain","plane","plant","plate","point","pound","power","press",
"price","pride","prime","print","prior","prize","proof","proud","prove","queen",
"quick","quiet","quite","radio","raise","range","rapid","ratio","reach","ready",
"refer","right","rival","river","rough","round","route","royal","rural","scale",
"scene","scope","score","sense","serve","seven","shall","shape","share","sharp",
"sheet","shelf","shell","shift","shirt","shock","shoot","short","shown","sight",
"since","sixth","sixty","sized","skill","sleep","slide","small","smart","smile",
"smith","smoke","solid","solve","sorry","sound","south","space","spare","speak",
"speed","spend","spent","split","spoke","sport","staff","stage","stake","stand",
"start","state","steam","steel","stick","still","stock","stone","stood","store",
"storm","story","strip","stuck","study","stuff","style","sugar","suite","super",
"sweet","table","taken","taste","taxes","teach","teeth","terry","texas","thank",
"their","theme","there","these","thick","thing","think","third","those","three",
"throw","tight","times","tired","title","today","topic","total","touch","tough",
"tower","track","trade","train","treat","trend","trial","tried","tries","truck",
"truly","trust","truth","twice","under","union","unity","until","upper","upset",
"urban","usage","usual","valid","value","video","virus","visit","vital","voice",
"waste","watch","water","wheel","where","which","while","white","whole","whose",
"woman","women","world","worry","worse","worst","worth","would","wound","write",
"wrong","wrote","yield","young","youth"
};

int word_count = sizeof(words) / sizeof(words[0]);

const char* get_random_word() {
    return words[os_rand() % word_count];
}

int wc_index = 0;
int win_check = 0;

void run_wordle() {
    os_srand(get_cpu_cycles());
    const char* word = get_random_word();
    println("=== Wordle ===");
    bool running = true;
    bool runningcheck = true;
    while (running) {
        char key = get_key();

        if (!key) {
            continue;
        }
        else if (isletter(key)) {
            putchar(key);
            w_buffer[w_index] = key;
            w_index++;
        }
        else if (key == 8) {
            if (w_index > 0) {
                w_index--;
                w_buffer[w_index] = '\0';
                cursorX--;
                putchar(' ');
                cursorX--;
            }
        }
        else if (key == '\n') {
            putchar('\n');
            if (strlength(w_buffer) == 5) {
                while (runningcheck)
                {
                    if (wc_index == 5) runningcheck = false;
                    else if (word[wc_index] == w_buffer[wc_index]) {
                        print("[G]");
                        wc_index++;
                        win_check++;
                    }
                    else if (contains(word, 5, w_buffer[wc_index])) {
                        print("[Y]");
                        wc_index++;
                    }
                    else {
                        print("[-]");
                        wc_index++;
                    }
                }
                running = false;
            }
            else {
                println("You need to a guess a 5 letter word!");
                for (int i = 0; i < W_BUFFER; i++) {
                    w_buffer[i] = 0;
                }
                wc_index = 0;
                w_index = 0;
            }
        }
        else {
            continue;
        }
    }
    
    putchar('\n');
    if (win_check == 5) {
        println("Correct!");
        wc_index = 0;
        w_index = 0;
        win_check = 0;
        running = false;
        runningcheck = false;
        for (int i = 0; i < W_BUFFER; i++) {
            w_buffer[i] = 0;
        }
        return;
    }
    wc_index = 0;
    w_index = 0;
    win_check = 0;
    running = true;
    runningcheck = true;
    for (int i = 0; i < W_BUFFER; i++) {
        w_buffer[i] = 0;
    }
    while (running) {
        char key = get_key();

        if (!key) {
            continue;
        }
        else if (isletter(key)) {
            putchar(key);
            w_buffer[w_index] = key;
            w_index++;
        }
        else if (key == 8) {
            if (w_index > 0) {
                w_index--;
                w_buffer[w_index] = '\0';
                cursorX--;
                putchar(' ');
                cursorX--;
            }
        }
        else if (key == '\n') {
            putchar('\n');
            if (strlength(w_buffer) == 5) {
                while (runningcheck)
                {
                    if (wc_index == 5) runningcheck = false;
                    else if (word[wc_index] == w_buffer[wc_index]) {
                        print("[G]");
                        wc_index++;
                        win_check++;
                    }
                    else if (contains(word, 5, w_buffer[wc_index])) {
                        print("[Y]");
                        wc_index++;
                    }
                    else {
                        print("[-]");
                        wc_index++;
                    }
                }
                running = false;
            }
            else {
                println("You need to a guess a 5 letter word!");
                for (int i = 0; i < W_BUFFER; i++) {
                    w_buffer[i] = 0;
                }
                wc_index = 0;
                w_index = 0;
            }
        }
        else {
            continue;
        }
    }
    putchar('\n');
    if (win_check == 5) {
        println("Correct!");
        wc_index = 0;
        w_index = 0;
        win_check = 0;
        running = true;
        runningcheck = true;
        for (int i = 0; i < W_BUFFER; i++) {
            w_buffer[i] = 0;
        }
        return;
    }
    wc_index = 0;
    w_index = 0;
    win_check = 0;
    running = true;
    runningcheck = true;
    for (int i = 0; i < W_BUFFER; i++) {
        w_buffer[i] = 0;
    }
    while (running) {
        char key = get_key();

        if (!key) {
            continue;
        }
        else if (isletter(key)) {
            putchar(key);
            w_buffer[w_index] = key;
            w_index++;
        }
        else if (key == 8) {
            if (w_index > 0) {
                w_index--;
                w_buffer[w_index] = '\0';
                cursorX--;
                putchar(' ');
                cursorX--;
            }
        }
        else if (key == '\n') {
            putchar('\n');
            if (strlength(w_buffer) == 5)
            {
                while (runningcheck)
                {
                    if (wc_index == 5) runningcheck = false;
                    else if (word[wc_index] == w_buffer[wc_index]) {
                        print("[G]");
                        wc_index++;
                        win_check++;
                    }
                    else if (contains(word, 5, w_buffer[wc_index])) {
                        print("[Y]");
                        wc_index++;
                    }
                    else {
                        print("[-]");
                        wc_index++;
                    }
                }
                running = false;
            }
            else {
                println("You need to a guess a 5 letter word!");
                for (int i = 0; i < W_BUFFER; i++) {
                    w_buffer[i] = 0;
                }
                wc_index = 0;
                w_index = 0;
            }
        }
        else {
            continue;
        }
    }
    putchar('\n');
    if (win_check == 5) {
        println("Correct!");
        wc_index = 0;
        w_index = 0;
        win_check = 0;
        running = true;
        runningcheck = true;
        for (int i = 0; i < W_BUFFER; i++) {
            w_buffer[i] = 0;
        }
        return;
    }
    wc_index = 0;
    w_index = 0;
    win_check = 0;
    running = true;
    runningcheck = true;
    for (int i = 0; i < W_BUFFER; i++) {
        w_buffer[i] = 0;
    }
    while (running) {
        char key = get_key();

        if (!key) {
            continue;
        }
        else if (isletter(key)) {
            putchar(key);
            w_buffer[w_index] = key;
            w_index++;
        }
        else if (key == 8) {
            if (w_index > 0) {
                w_index--;
                w_buffer[w_index] = '\0';
                cursorX--;
                putchar(' ');
                cursorX--;
            }
        }
        else if (key == '\n') {
            putchar('\n');
            if (strlength(w_buffer) == 5) {
                while (runningcheck)
                {
                    if (wc_index == 5) runningcheck = false;
                    else if (word[wc_index] == w_buffer[wc_index]) {
                        print("[G]");
                        wc_index++;
                        win_check++;
                    }
                    else if (contains(word, 5, w_buffer[wc_index])) {
                        print("[Y]");
                        wc_index++;
                    }
                    else {
                        print("[-]");
                        wc_index++;
                    }
                }
                running = false;
            }
            else {
                println("You need to a guess a 5 letter word!");
                for (int i = 0; i < W_BUFFER; i++) {
                    w_buffer[i] = 0;
                }
                wc_index = 0;
                w_index = 0;
            }
        }
        else {
            continue;
        }
    }
    putchar('\n');
    if (win_check == 5) {
        println("Correct!");
        wc_index = 0;
        w_index = 0;
        win_check = 0;
        running = true;
        runningcheck = true;
        for (int i = 0; i < W_BUFFER; i++) {
            w_buffer[i] = 0;
        }
        return;
    }
    wc_index = 0;
    w_index = 0;
    win_check = 0;
    running = true;
    runningcheck = true;
    for (int i = 0; i < W_BUFFER; i++) {
        w_buffer[i] = 0;
    }
    while (running) {
        char key = get_key();

        if (!key) {
            continue;
        }
        else if (isletter(key)) {
            putchar(key);
            w_buffer[w_index] = key;
            w_index++;
        }
        else if (key == 8) {
            if (w_index > 0) {
                w_index--;
                w_buffer[w_index] = '\0';
                cursorX--;
                putchar(' ');
                cursorX--;
            }
        }
        else if (key == '\n') {
            putchar('\n');
            if (strlength(w_buffer) == 5) {
                while (runningcheck)
                {
                    if (wc_index == 5) runningcheck = false;
                    else if (word[wc_index] == w_buffer[wc_index]) {
                        print("[G]");
                        wc_index++;
                        win_check++;
                    }
                    else if (contains(word, 5, w_buffer[wc_index])) {
                        print("[Y]");
                        wc_index++;
                    }
                    else {
                        print("[-]");
                        wc_index++;
                    }
                }
                running = false;
            }
            else {
                println("You need to a guess a 5 letter word!");
                for (int i = 0; i < W_BUFFER; i++) {
                    w_buffer[i] = 0;
                }
                wc_index = 0;
                w_index = 0;
            }
        }
        else {
            continue;
        }
    }
    putchar('\n');
    if (win_check == 5) {
        println("Correct!");
        wc_index = 0;
        w_index = 0;
        win_check = 0;
        running = true;
        runningcheck = true;
        for (int i = 0; i < W_BUFFER; i++) {
            w_buffer[i] = 0;
        }
        return;
    }
    else {
        println("Out of guesses! Word was:");
        print(word);
    }
    wc_index = 0;
    w_index = 0;
    win_check = 0;
    running = true;
    runningcheck = true;
    for (int i = 0; i < W_BUFFER; i++) {
        w_buffer[i] = 0;
    }
}
