#include <iostream>
#include <math.h>
#include <stdlib.h>
#include<string.h>
#include<msclr\marshal_cppstd.h>
#include <ctime>// include this header 
#include <mpi.h>
#pragma once

#using <mscorlib.dll>
#using <System.dll>
#using <System.Drawing.dll>
#using <System.Windows.Forms.dll>
using namespace std;
using namespace msclr::interop;

int* inputImage(int* w, int* h, System::String^ imagePath) //put the size of image in w & h
{
	int* input;


	int OriginalImageWidth, OriginalImageHeight;

	//********************Read Image and save it to local arrayss********	
	//Read Image and save it to local arrayss

	System::Drawing::Bitmap BM(imagePath);

	OriginalImageWidth = BM.Width;
	OriginalImageHeight = BM.Height;
	*w = BM.Width;
	*h = BM.Height;
	int* Red = new int[BM.Height * BM.Width];
	int* Green = new int[BM.Height * BM.Width];
	int* Blue = new int[BM.Height * BM.Width];
	input = new int[BM.Height * BM.Width];
	for (int i = 0; i < BM.Height; i++)
	{
		for (int j = 0; j < BM.Width; j++)
		{
			System::Drawing::Color c = BM.GetPixel(j, i);

			Red[i * BM.Width + j] = c.R;
			Blue[i * BM.Width + j] = c.B;
			Green[i * BM.Width + j] = c.G;

			input[i * BM.Width + j] = ((c.R + c.B + c.G) / 3); //gray scale value equals the average of RGB values

		}

	}
	return input;
}


void createImage(int* image, int width, int height, int index)
{
	System::Drawing::Bitmap MyNewImage(width, height);


	for (int i = 0; i < MyNewImage.Height; i++)
	{
		for (int j = 0; j < MyNewImage.Width; j++)
		{
			//i * OriginalImageWidth + j
			if (image[i * width + j] < 0)
			{
				image[i * width + j] = 0;
			}
			if (image[i * width + j] > 255)
			{
				image[i * width + j] = 255;
			}
			System::Drawing::Color c = System::Drawing::Color::FromArgb(image[i * MyNewImage.Width + j], image[i * MyNewImage.Width + j], image[i * MyNewImage.Width + j]);
			MyNewImage.SetPixel(j, i, c);
		}
	}
	MyNewImage.Save("..//Data//OutPut//5N.png");
	cout << "result Image Saved " << index << endl;
}


int main()
{
	int ImageWidth = 4, ImageHeight = 4;
	int start_s, stop_s, TotalTime = 0;
	System::String^ imagePath;
	std::string img;
	//img path to be tested
	img = "..//Data//Input//5N.png";
	imagePath = marshal_as<System::String^>(img);
	int* imageData = inputImage(&ImageWidth, &ImageHeight, imagePath); //pixel values of image
	start_s = clock();
	///start code here
	
	int kernel_size = 3;
	float** kernel = new float* [kernel_size];
	for (int i = 0; i < kernel_size; i++)kernel[i] = new float[kernel_size];
	//int kernel[3][3] = { {0,-1,0}, {-1,4,-1}, {0,-1,0} };

	//kernal multiplied by 10 to make pic more sharp

	kernel[0][0] = 0;
	kernel[0][1] = -10;
	kernel[0][2] = 0;
	kernel[1][0] = -10;
	kernel[1][1] = 40;
	kernel[1][2] = -10;
	kernel[2][0] = 0;
	kernel[2][1] = -10;
	kernel[2][2] = 0;
	
	//MPI Intialization .

	MPI_Init(NULL, NULL);
	//rank of processor 
	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	//size of processors
	int size;
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	//divide image into sub blocks to processors /// rows 
	// and store it's value in integer "no_of_divided_blocks"
	int no_of_divided_blocks = ImageHeight / size;
	//each processor have it's block form point "start" to point "end" of it's own
	int start = no_of_divided_blocks * rank;
	int end = start + no_of_divided_blocks;
	//if we are at last proc , end of blockes = image height
	if (rank == size - 1)
		end = ImageHeight;
	

	cout << "Processor of rank no." << rank << ' ' << "starts from : " << start << ' ' << "into : " << end << ' ' << endl;

	int root = 0;
	//no. of rows in each block
	int rows_of_Block = end - start;
	int divided_block_size = (rows_of_Block)*ImageWidth;
	int* ImageDataArray = new int[divided_block_size];
	

	//high pass calculation , we have 2 loops on block and 2 loops on kernal
	//loop on each block rows 
	for (int i = start; i < end; i++)
	{
		//loop on image coulmns
		for (int j = 0; j < ImageWidth; j++)
		{
			//(i,j) * kernel
			//Multiplying each pixel by kernal
			int new_valueOFpixel = 0; /// variable of each pixel of new image
			int x = -1;
			int y = -1;
			//loop on kernal rows
			for (int k = 0; k < kernel_size; k++)
			{

				//loop on kernal coulmns
				for (int l = 0; l < kernel_size; l++)
				{
					//if we exceed image width from left OR right
					if ((j + y >= ImageWidth) || (j + y < 0)) {
						y++;
						continue;
					}
					
					if ((rank == 0 && (i + x < start)) || ((rank == size - 1) && (i - start + x >= rows_of_Block))) {
						y++;
						continue;
					}


					// set new value of pixel = kernal *  pixel of tested pic
					new_valueOFpixel += kernel[k][l] * imageData[(i + x) * ImageWidth + (j + y)];
					y++;
				}
				x++;

				y = -1;
			}
			//collecting pixel into new array of blocks of image after high pass 
			ImageDataArray[(i - start) * ImageWidth + j] = new_valueOFpixel;
		}
	}

	if (rank == root)
	{
		// Define the receive counts
		
		int* counts = new int[size];
		int* displacement = new int[size];
		for (int i = 0; i < size; i++)
		{
			if (i == size - 1)
			{
				
				counts[i] = (ImageWidth * (ImageHeight)) - (divided_block_size * (size - 1));

				displacement[i] = (i)*divided_block_size;

				break;
			}
			displacement[i] = i * divided_block_size;
			counts[i] = divided_block_size;
		}



		MPI_Gatherv(ImageDataArray, divided_block_size, MPI_FLOAT, imageData, counts, displacement, MPI_FLOAT, root, MPI_COMM_WORLD);


	}
	else
	{
		MPI_Gatherv(ImageDataArray, divided_block_size, MPI_FLOAT, NULL, NULL, NULL, MPI_FLOAT, root, MPI_COMM_WORLD);
	}
	if (rank == 0) {
		createImage(imageData, ImageWidth, ImageHeight, 100);
	}
	stop_s = clock();
	TotalTime += (stop_s - start_s) / double(CLOCKS_PER_SEC) * 1000;
	if (rank == 0)
		cout << "time: " << TotalTime << endl;
	//MPI_Finalize();//////////////////////////////////////////////////////////////////
	///end
	MPI_Finalize();
	return 0;
}
