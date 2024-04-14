//時間計測あり逐次処理BPA
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>
#include <dirent.h>
#include <unistd.h>

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

//実行時刻を記録
unsigned long long getMilliseconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned long long milliseconds = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    return milliseconds;
}

//テキストファイルにunsigned long long型を追記，テキストファイルがない場合は作成する．また相対パスで指定できる．
void write_ull_to_file(const char* file_path, unsigned long long num) {
    FILE *file;

    // ファイルを追記モードで開く ('a' オプションは、ファイルが存在しない場合は新しいファイルを作成します)
    file = fopen(file_path, "a");

    if (file == NULL) {
        printf("エラー: ファイルを開けませんでした '%s'\n", file_path);
        return;
    }

    // unsigned long long をファイルに書き込む
    fprintf(file, "%llu\n", num);

    // ファイルを閉じる
    fclose(file);
}

//指定したフォルダ内のテキストファイルを全削除
void delete_text_files(const char* directory_path) {
    DIR *dir;
    struct dirent *entry;
    char filepath[1024];

    dir = opendir(directory_path);
    if (dir == NULL) {
        printf("エラー: ディレクトリを開けませんでした '%s'\n", directory_path);
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {  // レギュラーファイルなら
            // ファイル名が ".txt" で終わるか確認
            char *ext = strrchr(entry->d_name, '.');
            if (ext != NULL && strcmp(ext, ".txt") == 0) {
                snprintf(filepath, sizeof(filepath), "%s/%s", directory_path, entry->d_name);
                printf("テキストファイルを削除します: %s\n", filepath);
                if (unlink(filepath) == -1) {
                    printf("エラー: ファイルの削除に失敗しました '%s'\n", filepath);
                }
            }
        }
    }

    closedir(dir);
}

int main() {
    //delete_text_files("timedata/");
    unsigned long long start_time, end_time, elapsed_time;//全体時間計測変数
    start_time = getMilliseconds(); // 全体処理の開始時間を記録

    Point* point_list = NULL;
    int point_count = 0;//点群数格納
    int count = 0;//メッシュ数を数える
    //char *fpath = "/home/kit/mywork/myProgram/3ddata/usagi.obj";//objファイルのパス
    //char *fpath = "/home/kit/mywork/myProgram/3ddata/usagi0_003.obj";//objファイルのパス
    char *fpath = "/home/kit/mywork/myProgram/3ddata/usagi0_002.obj";//objファイルのパス


    unsigned long long file_start_time, file_end_time, file_elapsed_time;//ファイル読み込み時間計測変数
    file_start_time = getMilliseconds(); // ファイル読み込み処理の開始時間を記録
    extract_vertex_from_obj_file(fpath, &point_list, &point_count);//objファイルから点群座標取得
    file_end_time = getMilliseconds(); // 全体処理の終了時間を記録
    file_elapsed_time = file_end_time - file_start_time; // 全体経過時間を計算
    printf("ファイル読み込み時間: %llu milliseconds\n", file_elapsed_time);

    //double max_r = 0.00005;//外接円最大半径
    //double max_r = 0.007;//外接円最大半径，これくらいがちょうどいいかな？
    double max_r = 0.01;//外接円最大半径
    unsigned long long first_start_time, first_end_time, first_elapsed_time;//第一for文時間計測変数
    unsigned long long second_start_time, second_end_time, second_elapsed_time;//第二for文時間計測変数
    unsigned long long third_start_time, third_end_time, third_elapsed_time;//第三for文時間計測変数
    for (int i = 0; i < point_count; ++i) {
        first_start_time = getMilliseconds(); // 第一for文処理の開始時間を記録
        for (int j = i + 1; j < point_count; ++j) {
            second_start_time = getMilliseconds(); // 第二for文処理の開始時間を記録
            for (int k = j + 1; k < point_count; ++k) {
                //third_start_time = getMilliseconds(); // 第三for文処理の開始時間を記録
                double r = calculate_radius(point_list[i], point_list[j], point_list[k]);//外接円計算
                if (r <= max_r) {
                    printf("(%f, %f), (%f, %f), (%f, %f)\n",
                            point_list[i].x, point_list[i].y,
                            point_list[j].x, point_list[j].y,
                            point_list[k].x, point_list[k].y);
                    count ++;
                }
                //third_end_time = getMilliseconds(); // 第三for文処理の終了時間を記録
                //third_elapsed_time = third_end_time - third_start_time; // 第三for文経過時間を計算
                //write_ull_to_file("timedata/third_time.txt", third_elapsed_time);
            }
            second_end_time = getMilliseconds(); // 第二for文処理の終了時間を記録
            second_elapsed_time = second_end_time - second_start_time; // 第二for文経過時間を計算
            write_ull_to_file("timedata/second_time.txt", second_elapsed_time);
        }
        first_end_time = getMilliseconds(); // 第一for文処理の終了時間を記録
        first_elapsed_time = first_end_time - first_start_time; // 第一for文経過時間を計算
        write_ull_to_file("timedata/first_time.txt", first_elapsed_time);
    }
    free(point_list);//メモリ解放
    
    printf("点群数: %d\n", point_count);
    printf("メッシュ数: %d\n", count);

    end_time = getMilliseconds(); // 全体処理の終了時間を記録
    elapsed_time = end_time - start_time; // 全体経過時間を計算
    printf("全体時間: %llu milliseconds\n", elapsed_time);

    return 0;
}