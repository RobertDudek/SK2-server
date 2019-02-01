#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <cstdlib>
#include <stdlib.h>

#include <vector>
#include <iostream>
#include <algorithm>

#define SERVER_PORT 1306
#define QUEUE_SIZE 5
#define DELIMITER ";;"
#define MAX_ONGOING_GAMES 100

using namespace std;

#include "game.cpp"


//Globals, might change to shared in processed

vector<Quiz> quizesGlobal;
pthread_mutex_t quizesMutex;

vector<Game> gamesGlobal;
pthread_mutex_t gamesMutex;

vector<Game> onGoingGamesGlobal;
pthread_mutex_t onGoingGamesMutex;

vector<int> gamesIDsGlobal;
pthread_mutex_t gamesIDsMutex;


//struktura zawierająca dane, które zostaną przekazane do wątku
struct thread_data_t
{
  int clientSocketDescriptor;
  const char* nick;
};

vector<string> split(string data, string token)
{
    vector<string> output;
    size_t pos = string::npos; // size_t to avoid improbable overflow
    do
    {
        pos = data.find(token);
        output.push_back(data.substr(0, pos));
        if (string::npos != pos)
            data = data.substr(pos + token.size());
    } while (string::npos != pos);
    return output;
}



int sendall(int clientSD, string dataToSend)//, int *len)
{
  //uint32_t dataLength = htonl(dataToSend.size());
   // Send the data length
  cout<<endl<<endl;
  int n; //If we want to check how many bytes were send on error use this
  cout<<"sending "<<dataToSend<<" to client "<<clientSD<<endl;
  uint32_t dataLengthc = dataToSend.size();// htonl(dataToSend.size());
  cout<<"which is of lenght: "<<dataLengthc<<endl;
  char dataLength[5];
  sprintf(dataLength,"%04d",dataLengthc);
  dataLength[5] = '\0';
  cout<<"which is in chars: "<<dataLength<<endl;

  send(clientSD,&dataLength[0] ,sizeof(uint32_t) ,0);

  while(dataToSend != "") {
    cout<<"sending: "<<dataToSend<<endl;
      n = send(clientSD,dataToSend.c_str(),dataToSend.size(),0);   //.c_str() is to change into char*
      if (n == -1) { cout<<"oops, mistake in sending :("<<endl; break; }
      cout<<"we just send: "<<n<<endl;
      dataToSend = dataToSend.substr(n);
      cout<<"Left to Send: "<<dataToSend<<endl;
      cout<<endl;
  }

  // *len = n; // return number actually sent here
  cout<<"Finished Sending :)"<<endl;
  return n==-1?-1:0; // return -1 on failure, 0 on success

} 


int receiveall(int clientSD, string &receivedString){
  cout<<endl<<endl;
  char dataLengthc[5];
  int n = recv(clientSD,&dataLengthc[0],sizeof(char) * 4,0); // Receive the message length
  if(n == -1) return -1;
  if(n == 0) return 2; 
  dataLengthc[5] = '\0';
  
  int dataLength = atoi(dataLengthc);
  //cout<<"will read "<<dataLengthc<<" which is: "<<dataLength<<" beofore convert ot network"<<endl;
  //dataLength = ntohs(dataLength); // Ensure host system byte order
  cout<<"will read "<<dataLengthc<<" which is: "<<dataLength<<" in int"<<endl;
  if(dataLength < 1) return -1;
  vector<char> buffer;
  buffer.resize(dataLength,0);
  string rcv = "";   
  int bytesReceived = 0;
  do {
      cout<<"reading"<<endl;
      bytesReceived = recv(clientSD, &buffer[0], buffer.size()-bytesReceived, 0);
      cout<<"read "<<bytesReceived<<endl;
      if(n==0) {cout<<"bad disconnection :("<<endl; return 2;}
      // append string from buffer.
      if ( bytesReceived == -1 ) { 
          return -1; 
      } else {
          rcv.append( buffer.cbegin(), buffer.cend() ); //This has to be changed to get not a whole buffer, but only as much as we got I think (nvm, probably good)
          cout<<"read "<<rcv<<endl;
      }
  } while ( bytesReceived != dataLength );
  cout<<"message read end"<<endl;
  cout<<endl;
  receivedString = rcv;//.assign(&(rcvBuf[0]),rcvBuf.size()); // string
  return 0; // return -1 on failure, 0 on success, 2 on disconnect
}


int processCommand(string record, string me, int mySD){

    //
    string command = record.substr(0,2);
    cout<<"recognized command: "<< command<<" for "<<me<<endl;

    if(record.back() == '\n'){
      cout<<"cutting new line for nc testing purposes "<<endl;
      record = record.substr(0,record.length()-1);
    }


    //delete 2 first string chars here
    record = record.substr(2);
    cout<<"recognized record: "<<record<<endl;

    if(command == "nq"){
      //New Quiz command
      //QuizName;;Question;;Asnwer1;;Answer2;;Answer3;;Answer4;;Correct(number);;Qustion...

      vector<string> splitRecord = split(record,DELIMITER);

      Quiz q = Quiz(splitRecord[0]);
      cout<<"creating quiz with name(x2): l"<<splitRecord[0]<<"l l"<<q.getName()<<"l "<<endl;
      for(unsigned int i = 1; i+6<=splitRecord.size(); i+=6){
        q.addQuestion(splitRecord[i],splitRecord[i+1],splitRecord[i+2],splitRecord[i+3],splitRecord[i+4],stoi(splitRecord[i+5]));
      }

      pthread_mutex_lock(&quizesMutex);

        quizesGlobal.push_back(q);
      //apend new quiz to file db if I do this-------------------------------------------

      pthread_mutex_unlock(&quizesMutex);
    }
    
    if(command == "ng"){
      //New Game command
      //QuizName
      
      unsigned int quizIndex;
      Game g;
      pthread_mutex_lock(&quizesMutex);
      //Find the quiz index
        for(quizIndex = 0; quizIndex<quizesGlobal.size(); quizIndex++){
            if(quizesGlobal[quizIndex].getName() == record){
              break;
            }
            cout<<"l"<<quizesGlobal[quizIndex].getName()<<"l does not match l"<<record<<"l"<<endl;
          }
        
        if(quizIndex < quizesGlobal.size()){
          //Make sure there is no deadlock here, there can't be gamesIDsMutex over quizesMutex somewhere else
          pthread_mutex_lock(&gamesIDsMutex);
            if(gamesIDsGlobal.size()>0){
              g = Game(quizesGlobal[quizIndex],gamesIDsGlobal.back());
              gamesIDsGlobal.pop_back();
            }
            else{
          pthread_mutex_unlock(&gamesIDsMutex);
      pthread_mutex_unlock(&quizesMutex);
              sendall(mySD,"erSorry our server has reached its games limit, please try again later");
              return 0;
            }
          pthread_mutex_unlock(&gamesIDsMutex);
      pthread_mutex_unlock(&quizesMutex);
        }
        else {
      pthread_mutex_unlock(&quizesMutex);
          //er command send here
          sendall(mySD,"erAttempted to create a game of non-existent quiz");
          //This return doesn't tell about the error for now
          return 0;
        }
      pthread_mutex_unlock(&quizesMutex);
      
      pthread_mutex_lock(&gamesMutex);
        
        gamesGlobal.push_back(g);

      pthread_mutex_unlock(&gamesMutex);

      //change command and record to also join the game when you create it
      command = "jg";
      //g.getId() : id of newly added game
      record = to_string(g.getId());

    }
    if(command == "jg"){
      //Join Game command
      //IdOfTheGame
      int gotId = stoi(record);
      unsigned int gameIndex;
      //Find index of the game
      pthread_mutex_lock(&gamesMutex);
        for(gameIndex = 0; gameIndex < gamesGlobal.size(); gameIndex++){
          if(gamesGlobal[gameIndex].getId() == gotId){
            break;
          }
        }

        if(gameIndex < gamesGlobal.size()){
          gamesGlobal[gameIndex].addPlayer(me,mySD);
        }
        else {
      pthread_mutex_unlock(&gamesMutex);
          string erCommand = "erAttempted to join a non-existent game, try refreshing";
          sendall(mySD,erCommand);
          //This return doesn't tell about the error for now
          return 0;
        }
        pthread_mutex_unlock(&gamesMutex);
        //send info back to client 
        //Game Info command
        //GameID;;Player1Nick;;Player2Nick...
        command = "gp";

    }
    if(command == "lg"){
      //Leave Game command
      //IdOfTheGame
      int gotId = stoi(record);
      unsigned int gameIndex;

      pthread_mutex_lock(&gamesMutex);
        for(gameIndex=0;gameIndex<gamesGlobal.size();gameIndex++){
          if(gamesGlobal[gameIndex].getId() == gotId){
            gamesGlobal[gameIndex].removePlayer(me);
            break;
          }
        }
        if(gameIndex == gamesGlobal.size()){
      pthread_mutex_unlock(&gamesMutex);
        sendall(mySD,"erAttempted to leave a game you're not in");
        //This return doesn't tell about the error for now-----------------------------------------------------------------------
        return 0;
        }
        //Destroy the game locally, if player doesn's refresh and attepmts to join he will get simple please refresh error message
        if(gamesGlobal[gameIndex].isEmpty()){          
          gamesGlobal.erase(gamesGlobal.begin()+gameIndex);
        }
      pthread_mutex_unlock(&gamesMutex);
      
    }
    if(command == "sg"){
      //Start Game command
      //GameID

      int gotId = stoi(record);
      unsigned int gameIndex;

      pthread_mutex_lock(&gamesMutex);
        for(gameIndex=0;gameIndex<gamesGlobal.size();gameIndex++){
          if(gamesGlobal[gameIndex].getId() == gotId){
            break;
          }
        }
        if(gameIndex == gamesGlobal.size()){
      pthread_mutex_unlock(&gamesMutex);
          sendall(mySD,"erAttempted to start a non-existent or already started game");
          //This return doesn't tell about the error for now-----------------------------------------------------------------------
          return 0;
        }
        //Make sure there is no deadlock here, there can't be onGoingGamesMutex over gamesMutex somewhere else
        pthread_mutex_lock(&onGoingGamesMutex);
          onGoingGamesGlobal.push_back(gamesGlobal[gameIndex]);
          cout<<"Game of id "<<onGoingGamesGlobal.back().getId()<<" is now ongoing "<<endl;
          gamesGlobal.erase(gamesGlobal.begin()+gameIndex);
        pthread_mutex_unlock(&onGoingGamesMutex);
      pthread_mutex_unlock(&gamesMutex);




      //create ne thread for the game to handle nextQuestion, result sending etc.
      //create_result = pthread_create(&thread, NULL, GameThreadBehavior, (void *)t_data);
    }
    if(command == "gg"){
      //sg:
      //Send Games command
      //GameID;;numberOfPlayers;;GameID;;number...
      string sgCommand = "sg";
      pthread_mutex_lock(&gamesMutex);
        for(unsigned int i=0;i<gamesGlobal.size();i++){
          sgCommand += to_string(gamesGlobal[i].getId());
          sgCommand += DELIMITER;
          sgCommand += to_string(gamesGlobal[i].getNumberOfPlayers());
          sgCommand += DELIMITER;
        }
        //Delete last delimiter.
        string delimiterStr = DELIMITER; 
        sgCommand = sgCommand.substr(0, sgCommand.length() - delimiterStr.length() );
      pthread_mutex_unlock(&gamesMutex);
      sendall(mySD,sgCommand);
    }
    if(command == "gq"){
      //sq:
      //Send Quizes command
      //QuizName;;Question;;Asnwer1;;Answer2;;Answer3;;Answer4;;CorrectAnswer(1,2,3 or 4);;Question...
      string sqCommand = "sq";

      pthread_mutex_lock(&quizesMutex);
        for(unsigned int i=0;i<quizesGlobal.size();i++){
          sqCommand+=quizesGlobal[i].getName();
          sqCommand += DELIMITER;
          for(int j=0;j<quizesGlobal[i].getNumberOfQuestions();j++){
            vector<string> quest = quizesGlobal[i].getQuestionAsStringVector(j);
            for(unsigned int k=0;k<quest.size();k++){
              sqCommand+=quest[k];
              sqCommand+=DELIMITER;
            }
            sqCommand+=to_string(quizesGlobal[i].getCorrect(j));
            sqCommand+=DELIMITER;
          }
        }
        //Delete last delimiter
        string delimiterStr = DELIMITER; 
        sqCommand = sqCommand.substr(0, sqCommand.length() - delimiterStr.length() );

        sendall(mySD,sqCommand);
      pthread_mutex_unlock(&quizesMutex);

    }
    if(command == "gp"){
      //gp:
      //Get Players - returns Game Info(gi)
      //GameID
      cout<<"Sending game info for game id "<<record<<endl;
      int gotId = stoi(record);
      unsigned gameIndex;
      //cout<<"Stopped by mutex"<<endl;
      pthread_mutex_lock(&gamesMutex);
      //cout<<"Came in"<<endl;
        for(gameIndex = 0; gameIndex < gamesGlobal.size(); gameIndex++){
          if(gamesGlobal[gameIndex].getId() == gotId){
            break;
          }
        }
        //send back game info
        string giCommand = "gi";
        giCommand += record;
        giCommand += DELIMITER;

        vector<string> nicks = gamesGlobal[gameIndex].getNicks();
        
      pthread_mutex_unlock(&gamesMutex);
      //cout<<"Came out"<<endl;
      for(unsigned int i=0;i<nicks.size();i++){
        cout<<"Adding Player "<<nicks[i]<<" to gi command"<<endl;
        giCommand += nicks[i];
        giCommand += DELIMITER;
      }
        //Delete last delimiter.
        string delimiterStr = DELIMITER; 
        giCommand = giCommand.substr(0, giCommand.length() - delimiterStr.length() );

        cout<<"Will send the gi command"<<endl;
      sendall(mySD,giCommand);
    }
    if(command == "na"){
      //na:
      //New Answer
      //GameID;;ChosenAswer(1,2,3 or 4)
      vector<string> splitRecord = split(record,DELIMITER);
      int gotId = stoi(splitRecord[0]);
      unsigned int chosenAnswer= stoi(splitRecord[1]);
      unsigned int onGoingGameIndex;
      cout<<"New Answer: "<<chosenAnswer<<" for game id "<<gotId<<endl;

      //finding index of the game in onGoingGamesGlobal
      pthread_mutex_lock(&onGoingGamesMutex);
        for(onGoingGameIndex=0;onGoingGameIndex<onGoingGamesGlobal.size();onGoingGameIndex++){
          if(onGoingGamesGlobal[onGoingGameIndex].getId() == gotId){
            break;
          }
        }
        if(onGoingGameIndex == onGoingGamesGlobal.size()){
      pthread_mutex_unlock(&onGoingGamesMutex);
          sendall(mySD,"erAttempted to answer in a game that is not ongoing or doesn't exist");
          //This return doesn't tell about the error for now-----------------------------------------------------------------------
          return 0;
        }
        cout<<"found index of on-going game: "<<onGoingGameIndex<<endl;
        //finding index of the player in the game
        int myIndex;
        for(myIndex = 0; myIndex<onGoingGamesGlobal[onGoingGameIndex].getNumberOfPlayers(); myIndex++){
          if(onGoingGamesGlobal[onGoingGameIndex].getNick(myIndex) == me){
            break;
          }
        }
        if(myIndex == onGoingGamesGlobal[onGoingGameIndex].getNumberOfPlayers()){
      pthread_mutex_unlock(&onGoingGamesMutex);
          sendall(mySD,"erAttempted to answer in a game you are not in");
          return 0;
        }
        cout<<"Found myself on index: "<<myIndex<<endl;
        if(onGoingGamesGlobal[onGoingGameIndex].answer(myIndex, chosenAnswer)){
          //Next Qestion - send (Current Question or End Quiz) Command and Results Command
          bool queueDeletionOfQuiz = false;
          if(!(onGoingGamesGlobal[onGoingGameIndex].nextQuestion())){
            cout<<"End of quiz detected"<<endl;
            queueDeletionOfQuiz = true;
            //End of Quiz -send info to all players in game
            //eq:
            //End Quiz
            //GameID

              string eqCommand = "eq";
              eqCommand += onGoingGamesGlobal[onGoingGameIndex].getId();
              for(int i = 0;i<onGoingGamesGlobal[onGoingGameIndex].getNumberOfPlayers();i++){
              int cSD = onGoingGamesGlobal[onGoingGameIndex].getPlayerSD(i);

              sendall(cSD, eqCommand);
              }
              
            }
          
          else{
            //Send info about next question to all players in game
            //cq:
            //Current Question
            //GameID;;Question;;Answer1;;Answer2;;Answer3;;Answer4

              string cqCommand = "cq";
              cqCommand += DELIMITER;
              //splitRecord[0] still holds game ID
              cqCommand += splitRecord[0];
              vector<string> quest = onGoingGamesGlobal[onGoingGameIndex].getCurrentQuestion(); //returns vector<string> (5) in format we need

              for(unsigned int j = 0; j<quest.size();j++){
                cqCommand+=quest[j];
                cqCommand+=DELIMITER;
              }
              //Delete last delimiter.
              string delimiterStr = DELIMITER; 
              cqCommand = cqCommand.substr(0, cqCommand.length() - delimiterStr.length() );

              for(int i = 0;i<onGoingGamesGlobal[onGoingGameIndex].getNumberOfPlayers();i++){
                int cSD = onGoingGamesGlobal[onGoingGameIndex].getPlayerSD(i);

                sendall(cSD, cqCommand);
              }

            }
          //Send result/ final result
          string srCommand = "sr";
          //splitRecord[0] still has GameID
          srCommand += splitRecord[0]; 
          srCommand += DELIMITER;

          vector<Player> pl = onGoingGamesGlobal[onGoingGameIndex].getPlayers();

          sort(pl.begin(),pl.end(), [](Player a, Player b){return a.getResult() > b.getResult();} );

          for(unsigned int i = 0;i<pl.size();i++){
            srCommand += pl[i].getNick();
            srCommand += DELIMITER;
            srCommand += to_string(pl[i].getResult());
            srCommand += DELIMITER;
          }
          //Delete last delimiter.
          string delimiterStr = DELIMITER; 
          srCommand = srCommand.substr(0, srCommand.length() - delimiterStr.length() );

          for(int i = 0;i<onGoingGamesGlobal[onGoingGameIndex].getNumberOfPlayers();i++){
            int cSD = onGoingGamesGlobal[onGoingGameIndex].getPlayerSD(i);
            sendall(cSD,srCommand);
          }
          //Delete quiz at end so we don't lose information for funtions above
          if(queueDeletionOfQuiz){
            pthread_mutex_lock(&gamesIDsMutex);

              int newIdReady = onGoingGamesGlobal[onGoingGameIndex].getId();
              gamesIDsGlobal.push_back(newIdReady);
              
              onGoingGamesGlobal.erase(onGoingGamesGlobal.begin()+onGoingGameIndex);

            pthread_mutex_unlock(&gamesIDsMutex);

          }

        pthread_mutex_unlock(&onGoingGamesMutex);  

      }

    }

  return 0;
}

//function describing thread behavior - takes (void *) and returns (void *)
void *ThreadBehavior(void *t_data)
{
  pthread_detach(pthread_self());
  struct thread_data_t *th_data = (struct thread_data_t*)t_data;
  //dostęp do pól struktury: (*th_data).pole
  //cout<<"nick in new thread: "<<(*th_data).nick;
  string nickStr = (*th_data).nick;
  //cout<<"nick in new thread (string): "<<nickStr;
  //Main client handler loop
  int criticalError = 0;
  while(criticalError != 1){
    string message = "";
    cout<<"Receiving... "<<endl;
    if(receiveall((*th_data).clientSocketDescriptor,message) == 2){
      cout<<"connection broken"<<endl;
      pthread_exit(NULL);
    }

    cout<<"receiving now ended"<<endl;
    cout<<"Proccesing new message for: "<<(*th_data).clientSocketDescriptor<<endl;
    if(processCommand(message,nickStr,(*th_data).clientSocketDescriptor) == -1){
          cout<<"\n error proccesing command "<< message <<"  for player "<< (*th_data).nick << endl;
          criticalError = 1;
    }
    
  }




    pthread_exit(NULL);
}

//funkcja obsługująca połączenie z nowym klientem
void handleConnection(int connection_socket_descriptor) {
    //result of pthread_create function
    int create_result = 0;

    //thread handle
    pthread_t thread1;

    //data, that will be given to the thread
    //TODO dynamiczne utworzenie instancji struktury thread_data_t o nazwie t_data (+ w odpowiednim miejscu zwolnienie pamięci)
    struct thread_data_t t_data;
    memset(&t_data, 0, sizeof(struct thread_data_t));
    //TODO wypełnienie pól struktury
    t_data.clientSocketDescriptor = connection_socket_descriptor;

    //receive nick
    string nickStr;
    receiveall(connection_socket_descriptor,nickStr);
    
    if(nickStr.back() == '\n'){
      cout<<"cutting new line for nc testing purposes "<<endl;
      nickStr = nickStr.substr(0,nickStr.length()-1);
      cout<<"recognized nick "<<nickStr<<endl;
    }
    t_data.nick = nickStr.c_str();
    //put nick into t_data
  
    create_result = pthread_create(&thread1, NULL, ThreadBehavior, (void *)&t_data);
    if (create_result){
       printf("Error trying to create thread, error number: %d\n", create_result);
       exit(-1);
    }




  }

int main(int argc, char* argv[])
{
   int server_socket_descriptor;
   int connection_socket_descriptor;
   int bind_result;
   int listen_result;
   char reuse_addr_val = 1;
   struct sockaddr_in server_address;

   //inicjalizacja gniazda serwera
   
   memset(&server_address, 0, sizeof(struct sockaddr));
   server_address.sin_family = AF_INET;
   server_address.sin_addr.s_addr = htonl(INADDR_ANY);
   if(argc < 2){
   server_address.sin_port = htons(SERVER_PORT);
   } else{
    server_address.sin_port = htons(atoi(argv[1]));
   }
  
   server_socket_descriptor = socket(AF_INET, SOCK_STREAM, 0);
   if (server_socket_descriptor < 0)
   {
       fprintf(stderr, "%s: Error trying to create a socket\n", argv[0]);
       exit(1);
   }
   setsockopt(server_socket_descriptor, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse_addr_val, sizeof(reuse_addr_val));

   bind_result = bind(server_socket_descriptor, (struct sockaddr*)&server_address, sizeof(struct sockaddr));
   if (bind_result < 0)
   {
       fprintf(stderr, "%s: Error trying to bind IP and port do the socket\n", argv[0]);
       exit(1);
   }

   listen_result = listen(server_socket_descriptor, QUEUE_SIZE);
   if (listen_result < 0) {
       fprintf(stderr, "%s: Error trying to set QUEUE_SIZE\n", argv[0]);
       exit(1);
   }

    if (pthread_mutex_init(&quizesMutex, NULL) != 0)
    {
        printf("\n quizes mutex init failed\n");
        return 1;
    }
    if (pthread_mutex_init(&gamesMutex, NULL) != 0)
    {
        printf("\n games mutex init failed\n");
        return 1;
    }
    if (pthread_mutex_init(&onGoingGamesMutex, NULL) != 0)
    {
        printf("\n on going games mutex init failed\n");
        return 1;
    }
    if (pthread_mutex_init(&gamesIDsMutex, NULL) != 0)
    {
        printf("\n games IDs mutex init failed\n");
        return 1;
    }

    for(int i = MAX_ONGOING_GAMES;i>0;i--){
      gamesIDsGlobal.push_back(i);
    }

   while(1)
   {
       connection_socket_descriptor = accept(server_socket_descriptor, NULL, NULL);
       if (connection_socket_descriptor < 0)
       {
           fprintf(stderr, "%s: Error trying to accept a connection\n", argv[0]);
           exit(1);
       }

       handleConnection(connection_socket_descriptor);
   }

   close(server_socket_descriptor);
   return(0);
}
