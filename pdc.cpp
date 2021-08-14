#include <map>
#include <omp.h>
#include <mpi.h>
#include <string>
#include <vector>
#include <crypt.h>
#include <fstream>
#include <string.h>
#include <unistd.h>
#include <iostream>

#include "crack.cpp"

using namespace std;

int main(int argc, char** argv)
{
	// driver code for the program -> responsible for starting the processes, distributing the alphabet among them, calling on them to search for the password,
	// print the password once it is cracked, and then terminate all processes and end program

	int rank, nprocs;

	// MPI initialisation
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	// master specific code
	if (rank == 0) {
		cout << "master: there are " << nprocs - 1 << " slave processes" << endl;

		string username = "project";

		string salt_hash = get_salt_hash("/mirror/shadow.txt", username);	// parse file and extract the salt + hash for the specified username

		if (salt_hash.empty()) {
			cout << "File was not opened, terminating program." << endl;
			MPI_Abort(MPI_COMM_WORLD, 0); // terminates all MPI processes associated with the mentioned communicator (return value 0)
			exit(0);	// just in case :)
		}

		map<int, string> distrib = divide_alphabet(nprocs-1);	// getting the alphabet division per process (including master, if required)

		// send salt_hash and the letters assigned to that process to every slave
		for (int i = 1; i < nprocs; i++) {
			MPI_Send(salt_hash.c_str(), 200, MPI_CHAR, i, 100, MPI_COMM_WORLD);	// send the salt + hash string to every slave
			MPI_Send(distrib[i].c_str(), 30, MPI_CHAR, i, 101, MPI_COMM_WORLD);	// send every slave their allocated characters
		}

		// some characters have been allotted to master
		if (distrib[0].length() > 0) {	
			string letters = distrib[0];
			cout << "master: " << letters << endl;

			// master needs two threads running in parallel here
			// one to search through its own assigned characters
			// one to wait in case any slave cracks the password and reports to master
			#pragma omp parallel num_threads(2)
			{
				// 1st thread: search through the permuations of it's letters
				if (omp_get_thread_num() == 0) {
					// go through its assigned characters -> for every character, call the cracking function on it
					for (int i = 0; i < letters.length(); i++) {
						password_cracker(letters[i], salt_hash);
					}
				}

				// 2nd thread: wait for slave to find the answer instead
				// note: even if master finds the password, it also sends a signal back to itself (and recieves it here)
				else {
					char password[10];

					MPI_Status status;
					MPI_Recv(password, 10, MPI_CHAR, MPI_ANY_SOURCE, 200, MPI_COMM_WORLD, &status);	// blocking recieve waiting for any process to report
					
					cout << "Process " << status.MPI_SOURCE << " has cracked the password: " << password << endl;
					cout << "Terminating all processes" << endl;

					MPI_Abort(MPI_COMM_WORLD, 0); // terminates all MPI processes associated with the mentioned communicator (return value 0)
					// this is not a graceful way of exiting but will suffice for the functionality we require
				}
			}
		}
		
		// in this case, the characters have been divided equally among the slaves
		// master has nothing to do except wait for a slave to report password found
		else {
			char password[10];

			MPI_Status status;
			MPI_Recv(password, 10, MPI_CHAR, MPI_ANY_SOURCE, 200, MPI_COMM_WORLD, &status);	// blocking recieve waiting for any process to report
			
			cout << "Process " << status.MPI_SOURCE << " has cracked the password: " << password << endl;
			cout << "Terminating all processes" << endl;

			MPI_Abort(MPI_COMM_WORLD, 0);	// terminates all MPI processes associated with the mentioned communicator (return value 0)
			// this is not a graceful way of exiting but will suffice for the functionality we require
		}
	}

	// slave specific code
	else {
		char salt_hash[200], letters[30];

		MPI_Recv(salt_hash, 200, MPI_CHAR, 0, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);	// recieve salt_hash (same for every slave)
		MPI_Recv(letters, 200, MPI_CHAR, 0, 101, MPI_COMM_WORLD, MPI_STATUS_IGNORE);	// recieve allotted characters (different for every slave)

		cout << "slave " << rank << ": " << letters << endl;

		sleep(2);	// this is just to ensure all slaves print their allotted characters then start execution (for neatness :) )

		// go through its assigned characters -> for every character, call the cracking function on it
		for (int i = 0; i < charToString(letters).length(); i++) {
			password_cracker(letters[i], charToString(salt_hash));
		}	
	}

	MPI_Finalize();

	return 0;
}
