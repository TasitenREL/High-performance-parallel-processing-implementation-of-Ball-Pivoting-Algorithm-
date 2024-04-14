//時間を計測する並列処理BPA
//更にこれはOBJファイルを書き出すプログラム
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
//#include "mpi.h"
#include <omp.h>

// 座標を表す構造体
typedef struct {
    double x;
    double y;
} Point;

// 三角形の辺の長さを表す構造体
typedef struct {
    double a_edge;
    double b_edge;
    double c_edge;
} Edge;

typedef struct Mesh {
    int p1;
    int p2;
    int p3;
} Mesh;

// 二点間の距離を計算する関数
double distance(Point a, Point b) {
    return sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}

// 外接円の半径を計算する関数
double calculate_radius(Point a, Point b, Point c) {
    // 三辺の長さを計算
    double a_edge = distance(a, b);
    double b_edge = distance(b, c);
    double c_edge = distance(c, a);

    // ヘロンの公式による面積計算
    double s = (a_edge + b_edge + c_edge) / 2;
    double area = sqrt(s * (s - a_edge) * (s - b_edge) * (s - c_edge));

    // 外接円の半径計算
    double R = a_edge * b_edge * c_edge / (4 * area);
    return R;
}

// objファイルから頂点座標情報だけを取り出す関数
void extract_vertex_from_obj_file(const char* file_path, Point** point_list, int* point_count) {
    FILE *file = fopen(file_path, "r");
    if (file == NULL) {
        printf("Unable to open file.\n");
        return;
    }

    char line[256];
    Point temp_points[10000];
    *point_count = 0;

    while (fgets(line, sizeof(line), file)) {//fileから一行ずつ文字列を読み取り，lineに格納
        if (line[0] == 'v' && line[1] == ' ') {//文字列の先頭がvのみの文字列であるか
            Point point;
            double y_point;
            sscanf(line, "v %lf %lf %lf", &point.x, &y_point, &point.y);//文字列からxとz座標のみ抽出する．%*lfと書くと抽出が飛ばされる
            temp_points[(*point_count)++] = point;//配列に格納
        }
    }

    fclose(file);

    *point_list = (Point*)malloc(sizeof(Point) * (*point_count));//メモリ割り当て
    memcpy(*point_list, temp_points, sizeof(Point) * (*point_count));//point_listへtemp_pointsをコピー
}

//テキストファイルにdouble型の配列を書き込む
void writeNumbersToFile(const double* numbers, int numCount, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        printf("ファイルを開くことができませんでした。\n");
        return;
    }

    for (int i = 0; i < numCount; i++) {
        fprintf(file, "%f\n", numbers[i]);
    }

    fclose(file);
}

// 頂点データと面データをOBJファイルに書き出す関数
void write_obj_file(const char *filename, Point *vertices, int num_vertices, Mesh *faces, int num_faces) {
    FILE *fp;
    fp = fopen(filename, "w");

    if (fp == NULL) {
        printf("Error opening file\n");
        return;
    }

    // 頂点データを書き出す
    for(int i = 0; i < num_vertices; i++) {
        fprintf(fp, "v %f %f %f\n", vertices[i].x, 0.000, vertices[i].y);
    }

    // 単位法線を
    fprintf(fp, "vn -0.0000 1.0000 0.0000\n");

    // 面データを書き出す
    for(int i = 0; i < num_faces; i++) {
        fprintf(fp, "f %d//1 %d//1 %d//1\n", faces[i].p1 + 1, faces[i].p2 + 1, faces[i].p3 + 1); // 1-indexed
    }
    fclose(fp);
}

void print_meshes(Mesh* meshes, int mesh_count) {
    for (int i = 0; i < mesh_count; i++) {
        printf("Mesh %d: p1 = %d, p2 = %d, p3 = %d\n", i, meshes[i].p1, meshes[i].p2, meshes[i].p3);
    }
}

int main( argc, argv )
int argc;
char **argv;
{
    double time_list[16][3000];

    Point* point_list = NULL;
    int point_count = 0;//点数を数える
    //char *fpath = "/home/kit/GitCode_mywork/High-performance_parallel_processing/myProgram/3ddata/usagi.obj";//objファイルのパス
    //char *fpath = "/home/kit/GitCode_mywork/High-performance_parallel_processing/myProgram/3ddata/usagi0_006.obj";//objファイルのパス
    //char *fpath = "/home/kit/GitCode_mywork/High-performance_parallel_processing/myProgram/3ddata/usagi0_004.obj";//objファイルのパス
    char *fpath = "/home/kit/GitCode_mywork/High-performance_parallel_processing/myProgram/3ddata/usagi0_003.obj";//objファイルのパス
    //char *fpath = "/home/kit/GitCode_mywork/High-performance_parallel_processing/myProgram/3ddata/usagi0_002.obj";//objファイルのパス

    extract_vertex_from_obj_file(fpath, &point_list, &point_count); //objファイルから点座標取得

    double max_r = 0.01; //外接円最大半径

    #pragma omp parallel//並列スレッド生成
    {
        double start_time, end_time, elapsed_time;
        double first_start_time, first_end_time, first_elapsed_time;
        int i,j,k;
        int loop_count = 0;

        start_time = omp_get_wtime(); // 全体処理の開始時間を記録
        int myid, nthreads;
        nthreads = omp_get_num_threads();
        myid = omp_get_thread_num();

        #pragma omp for private(j,k)//forループをスレッドで分割し、並列処理を行う
        for (i=0; i<point_count; ++i) {
            first_start_time = omp_get_wtime(); // 第一for文処理の開始時間を記録
            for (j= i+1; j<point_count; ++j) {
                for (k=j+1; k<point_count; ++k) {
                    double r = calculate_radius(point_list[i], point_list[j], point_list[k]); //外接円計算
                    if (r <= max_r) {
                        // メッシュを作成し、配列に格納
                        Mesh mesh;
                        mesh.p1 = i;
                        mesh.p2 = j;
                        mesh.p3 = k;
                        //printf("myid:%d, start[i]:%d, start[j]:%d, start[k]:%d\n",myid,i,j,k);
                    }
                }
            }
            //printf("%d\n",loop_count);
            first_end_time = omp_get_wtime(); // 第一for文処理の終了時間を記録
            first_elapsed_time = first_end_time - first_start_time; // 第一for文経過時間を計算
            //printf("rank%dの全体時間: %f seconds\n", myid, first_elapsed_time);
            time_list[myid][loop_count] = first_elapsed_time;
            loop_count++;
        }
        end_time = omp_get_wtime(); // 全体処理の終了時間を記録
        elapsed_time = end_time - start_time; // 全体経過時間を計算
        if (myid == 0){
            printf("点数: %d\n", point_count);
        }
        printf("rank%dの全体時間: %f seconds\n", myid, elapsed_time);
        //printf("%d\n",loop_count);
        //時間を書き出す
        char TimeFpath[32];
        sprintf(TimeFpath, "timedata/first_time%d.txt", myid);
        writeNumbersToFile(time_list[myid], loop_count, TimeFpath);
    }
    return 0;
}