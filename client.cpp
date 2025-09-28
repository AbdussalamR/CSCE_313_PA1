/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name: Abbdussalam Raheem
	UIN: 434000596
	Date: 09/23/2025
*/
#include <iostream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <chrono>
#include <iomanip>


#include "common.h"
#include "FIFORequestChannel.h"

using namespace std;

// Helper func for req a file
void request_file(FIFORequestChannel* chan, const string& filename, int buffercapacity) {
	// request file size
	filemsg fm_size(0,0);
	int len_msg_size = sizeof(filemsg) + filename.size() + 1;
	char* buf_size = new char[len_msg_size];
	memcpy(buf_size, &fm_size, sizeof(filemsg));
	strcpy(buf_size + sizeof(filemsg), filename.c_str());

	chan->cwrite(buf_size, len_msg_size);
	delete[] buf_size;

	__int64_t file_size;
	chan->cread(&file_size, sizeof(__int64_t));
	cout << "Client: Server returned file size of " << file_size << " bytes for " << filename << endl;

	// request file in chunks
	string output_filepath = "received/" + filename;
    FILE* outfile = fopen(output_filepath.c_str(), "wb"); // "wb" for binary write
    if (!outfile) {
        perror(("Error opening " + output_filepath).c_str());
        return;
    }

	cout << "Client: Starting file transfer..." << endl;
    auto start = chrono::high_resolution_clock::now();

    __int64_t bytes_rem = file_size;
    __int64_t offset = 0;
    char* recv_buffer = new char[buffercapacity];

    while (bytes_rem > 0) {
        int chunk_size = min((__int64_t)buffercapacity, bytes_rem);
        
        filemsg fm_chunk(offset, chunk_size);
        int len_msg_chunk = sizeof(filemsg) + filename.size() + 1;
        char* buf_chunk = new char[len_msg_chunk];
        memcpy(buf_chunk, &fm_chunk, sizeof(filemsg));
        strcpy(buf_chunk + sizeof(filemsg), filename.c_str());
        
        chan->cwrite(buf_chunk, len_msg_chunk);
        delete[] buf_chunk;

        chan->cread(recv_buffer, chunk_size);
        fwrite(recv_buffer, 1, chunk_size, outfile);

        offset += chunk_size;
        bytes_rem -= chunk_size;
    }

    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double> diff = end - start;

    delete[] recv_buffer;
    fclose(outfile);
    cout << "Client: File transfer complete. Received " << file_size << " bytes." << endl;
    cout << "Time taken: " << diff.count() << " seconds." << endl;
}

// Helper func for req 1000 data points

void request_1000_data_points(FIFORequestChannel* chan, int p) {
	cout << "Request 1000 data points for patient " << p << endl;

	ofstream ofs("received/x1.csv");
	if (!ofs.is_open()) {
		cerr << "Error opening file x1.csv for writing." << endl;
		return;
	}

	for (int i = 0; i < 1000; ++i) {
		double time = i * 0.004;
		// Request ecg1
		datamsg d1(p, time, 1);
		chan->cwrite(&d1, sizeof(datamsg));
		double ecg1_val;
		chan->cread(&ecg1_val, sizeof(double));


		//Request ecg2
		datamsg d2(p, time, 2);
		chan->cwrite(&d2, sizeof(datamsg));
		double ecg2_val;
		chan->cread(&ecg2_val, sizeof(double));

		ofs << time << "," << ecg1_val << "," << ecg2_val << endl;

	}

	ofs.close();
	cout << "Finished writing 1000 data points to x1.csv" << endl;
}



int main (int argc, char *argv[]) {
	int opt;
	int p = -1;
	double t = -1.0;
	int e = -1;
	int buffercapacity = MAX_MESSAGE; 
	bool new_channel_flag = false;
	
	string filename = "";
	while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
		switch (opt) {
			case 'p':
				p = atoi (optarg);
				break;
			case 't':
				t = atof (optarg);
				break;
			case 'e':
				e = atoi (optarg);
				break;
			case 'f':
				filename = optarg;
				break;
			case 'm':
				buffercapacity = atoi(optarg);
				break;
			case 'c':
				new_channel_flag = true;
				break;
		}
	}

	// Fork to run server as child
	pid_t pid = fork();

	if (pid < 0) {
		cerr << "fork failed\n";
		return 1;
	}

	if (pid == 0) { //child process
		// Don't pass all client's arguments to server, only pass what it needs -m and its value
		char* server_args[4];
		server_args[0] = (char*)"./server";  // name of program
		string m_arg = to_string(buffercapacity);
		server_args[1] = (char*)"-m";
		server_args[2] = (char*)m_arg.c_str();
		server_args[3] = NULL; // Needs to end with null

		execvp(server_args[0], server_args);
		perror("execvp failed");
		exit(1);
	}

	// Parent process (client) continues here

    FIFORequestChannel* c_chan = new FIFORequestChannel("control", FIFORequestChannel::CLIENT_SIDE);
	FIFORequestChannel* chan = c_chan;

	if (new_channel_flag) {
		cout << "Client: requesting a new channel" << endl;
		MESSAGE_TYPE new_chan_msg = NEWCHANNEL_MSG;
		c_chan->cwrite(&new_chan_msg, sizeof(MESSAGE_TYPE));
		

		char new_chan_name[100]; // reciece new channel name
		c_chan->cread(new_chan_name, sizeof(new_chan_name));
		chan = new FIFORequestChannel(new_chan_name, FIFORequestChannel::CLIENT_SIDE);
		cout << "Client: created new channel: " << new_chan_name << endl;
	}

	if(!filename.empty()) {
		// request a file
		// made helper function to do this
		request_file(chan, filename, buffercapacity);
	} else if (p != -1) {
		if (t != -1.0) {
			// request single data point
			datamsg d(p, t, e);
			chan ->cwrite(&d, sizeof(datamsg));

			double reply;
			chan->cread(&reply, sizeof(double));
			cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
		} else {
			// request first 1000 dp
			request_1000_data_points(chan, p);
		}
	}

	MESSAGE_TYPE m = QUIT_MSG;
	chan->cwrite(&m, sizeof(MESSAGE_TYPE));

	// if new channel was created, close c_chan
	if(new_channel_flag) {
		c_chan->cwrite(&m, sizeof(MESSAGE_TYPE));
		delete chan;
	}

	delete c_chan;

	// wait for server to process to terminate
	wait(NULL);
    cout << "Client finished" << endl;

	return 0;

	
	// // example data point request
    // char buf[MAX_MESSAGE]; // 256
    // datamsg x(1, 0.0, 1);
	
	// memcpy(buf, &x, sizeof(datamsg));
	// chan.cwrite(buf, sizeof(datamsg)); // question
	// double reply;
	// chan.cread(&reply, sizeof(double)); //answer
	// cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
	
    // // sending a non-sense message, you need to change this
	// filemsg fm(0, 0);
	// string fname = "teslkansdlkjflasjdf.dat";
	
	// int len = sizeof(filemsg) + (fname.size() + 1);
	// char* buf2 = new char[len];
	// memcpy(buf2, &fm, sizeof(filemsg));
	// strcpy(buf2 + sizeof(filemsg), fname.c_str());
	// chan.cwrite(buf2, len);  // I want the file length;

	// delete[] buf2;
	
	// // closing the channel    
    // MESSAGE_TYPE m = QUIT_MSG;
    // chan.cwrite(&m, sizeof(MESSAGE_TYPE));
}
