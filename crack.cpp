#include <iostream>
#include <string>
#include <mpi.h>
#include <crypt.h>
#include <string.h>

#include "utility.cpp"

using namespace std;

bool are_all_Z(string str) {

    // this function is used to check if a string has only zzzzzz (first character excepted)
    // done by checking string from 2nd character to the end

    // for the edge case of string having only one character
    if (str.length() == 1) {
        if (str[0] == 'z')
            return true;
        return false;
    }

    // check that all characters save the starting one are z
    for (int i = 1; i < str.length(); i++) {
        if (str[i] != 'z')
            return false;
    }

    return true;
}

void password_cracker(const char initial, string salt_hash) {

    // initial: the character whose possibilities have to be traversed e.g. a -> a, ab, ac...abbbd, abbbe... till azzzzzzzz 
    // this function traverses through the character's strings, crypts each using the salt and compares the resulting hash to the salt_hash parameter
    // if password is found, function prints result, sends password to master (rank 0 proc) then terminates

    /********************************************************************************************************/

    bool found = false; // used to keep track of password being found or not

    int current_length = 0; // keep track of which length possibilities are being explored -> 2 characters, 3 characters and so on
    const int MAX_LENGTH = 8;   // maximum string-length to check possibilities upto

    string initial_char = "";   // string representation of the initial char parameter
    initial_char += initial;
    string current = initial_char;  // initialising the current string with the initial character

    /*********************************************************************************************************/

    vector<string> tokens = split_string(salt_hash, '$'); // split by dollar sign to split up salt and hash
	string salt = "$" + tokens[1] + "$" + tokens[2] + "$"; // get salt and hash

    // run until password is found or max length is exceeded
    while (!found and current_length < MAX_LENGTH) {

        // incrementing characters till we reach end e.g. azz or azzzz
        while (!are_all_Z(current) and !found) {
        
        	cout << "current: " << current << endl;

            // crypt the current string with constant salt
            string crypted = crypt(current.c_str(), salt.c_str());  
            
            // compare the hash generated to the salt_hash
            if (crypted == salt_hash) {
                found = true;
                cout << "password is " << current << endl;

                // signal master that password has been found (and send the password along)            
                MPI_Send(current.c_str(), 10, MPI_CHAR, 0, 200, MPI_COMM_WORLD);   
                break;
            }

            else { 
                bool changed = false;

                // go backwards in the string till a non-z character is found
                for (int i = current.length()-1; i >= 0 and !changed; i--) {
                    // convert any z encountered to a (resetting it)
                    if (current[i] == 'z') {
                        current[i] = 'a';
                        continue;
                    }

                    // incrementing the first non-z character found from the right
                    if (current[i] != 'z') {
                        current[i]++;

                        // as we only change one character at a time, so we mark that it has been changed
                        changed = true;
                    }
                }
            }
        }

        cout << "current: " << current << endl;

        // this is so we can check for an all-z string e.g. azz, bzzz
        // if this was not applied, z-strings would be skipped
        string crypted = crypt(current.c_str(), salt.c_str());

        if (crypted == salt_hash and !found) {
            found = true;
            cout << "password is " << current << endl;
            
            // signal master that password has been found (and send the password along) 
            MPI_Send(current.c_str(), 10, MPI_CHAR, 0, 200, MPI_COMM_WORLD);
            
            break;
        }

        // string length has to added to now
        current_length++;
        
        // resetting the string and incrementing length
        current = initial_char;
        for (int i = 0; i < current_length; i++)
            current += 'a';
    }
}
