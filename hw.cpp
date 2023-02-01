#define _CRT_SECURE_NO_WARNINGS
#define REMAP(s, a, b, c, d) LERP(INVERSE_LERP(s, a, b), c, d)
#include "snail.cpp"
#include "cow.cpp"
#include "_cow_supplement.cpp"
#include <time.h>

double g = -9.81;
double m = 1;
double init_v = .01;
double box = 20.;

BasicTriangleMesh3D *mesh;
vec3 light_p = V3(0, 2.5, 0);

int S = 75;
Texture color_buffer;
void raycast_set_pixel(int i, int j, vec3 color)
{
    color_buffer.data[4 * (j * S + i) + 0] = (unsigned char)(255 * CLAMP(color.r, 0, 1));
    color_buffer.data[4 * (j * S + i) + 1] = (unsigned char)(255 * CLAMP(color.g, 0, 1));
    color_buffer.data[4 * (j * S + i) + 2] = (unsigned char)(255 * CLAMP(color.b, 0, 1));
    color_buffer.data[4 * (j * S + i) + 3] = 255;
}

struct CastRayResult
{
    bool hit_at_least_one;
    vec3 base_color;
    double min_t;
    vec3 n_hit;
    vec3 p_hit;
};

CastRayResult cast_ray(vec3 dir, vec3 o)
{
    bool hit_at_least_one = false;
    vec3 base_color = {};
    double min_t = INFINITY;
    vec3 n_hit = {};
    vec3 p_hit = {};

    int num_triangles = mesh->num_vertices / 3;
    for (int triangle_i = 0; triangle_i < num_triangles; triangle_i++)
    {
        vec3 a, b, c;
        vec3 color_a, color_b, color_c;
        vec3 n;
        {
            a = mesh->vertex_positions[3 * triangle_i + 0];
            b = mesh->vertex_positions[3 * triangle_i + 1];
            c = mesh->vertex_positions[3 * triangle_i + 2];
            vec3 e1 = b - a;
            vec3 e2 = c - a;
            n = normalized(cross(e1, e2));
            if (mesh->vertex_colors != NULL)
            {
                color_a = mesh->vertex_colors[3 * triangle_i + 0];
                color_b = mesh->vertex_colors[3 * triangle_i + 1];
                color_c = mesh->vertex_colors[3 * triangle_i + 2];
            }
            else
            {
                vec3 fallback_color = V3(.5, .5, .5) + .5 * n;
                color_a = fallback_color;
                color_b = fallback_color;
                color_c = fallback_color;
            }
        }

        mat4 coords = {a.x, b.x, c.x, -dir.x, a.y, b.y, c.y, -dir.y, a.z, b.z, c.z, -dir.z, 1, 1, 1, 0};
        vec4 os = {o.x, o.y, o.z, 1};
        vec4 bary = inverse(coords) * os;
        double tiny = .0000000001;
        bool hit = (bary[0] > tiny && bary[1] > tiny && bary[2] > tiny && bary[3] > tiny);
        if (hit)
        {
            hit_at_least_one = true;
            if (bary[3] < min_t)
            {
                min_t = bary[3];
                base_color = (bary[0] * color_a) + (bary[1] * color_b) + (bary[2] * color_c);
                n_hit = n;
            }
        }
    }
    p_hit = o + min_t * dir;
    return {hit_at_least_one, base_color, min_t, n_hit, p_hit};
}

struct Ball
{
    vec3 position;
    vec3 color;
    vec3 velocity;
    vec3 force;
};

const int N = 100;

Ball *balls = (Ball *)malloc(N * sizeof(Ball));

void init_balls()
{
    srand(time(NULL));

    for (int i = 0; i < N; i++)
    {
        Ball *ball_i = &balls[i];

        double x = 2 * box * (double(rand()) / RAND_MAX) - box;
        double y = 20 * (double(rand()) / RAND_MAX) + 5;
        double z = 2 * box * (double(rand()) / RAND_MAX) - box;

        double xc = (double)rand() / RAND_MAX;
        double yc = (double)rand() / RAND_MAX;
        double zc = (double)rand() / RAND_MAX;

        ball_i->position = {x, y, z};
        ball_i->color = {xc, yc, zc};
        ball_i->velocity = {xc, yc, zc};
    }
}

void final()
{
    bool playing = false;
    FPSCamera ferret = {V3(0, 1, 0), RAD(60), RAD(180), 0};
    init_balls();

    while (begin_frame())
    {
        // gui
        imgui_checkbox("playing", &playing, 'p');

        static int num_balls = 4;
        int min = 4;
        int max = 100;
        imgui_slider("VERTICES", &num_balls, min, max, 0, 0, true);

        static int size = 2;
        int min_size = 1;
        int max_size = 5;
        imgui_slider("SIZE", &size, min_size, max_size, 0, 0, true);

        static int mass = 1.;
        int min_mass = 0;
        int max_mass = 5.;
        imgui_slider("MASS", &mass, min_mass, max_mass, 0, 0, true);

        if (widget_active_widget_ID == 0 && input.mouse_left_pressed)
        {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        else if (input.key_pressed[KEY_ESCAPE])
        {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }

        // cameras
        mat4 C = fps_camera_get_C(&ferret);
        mat4 P = tform_get_P_perspective(ferret.angle_of_view);
        mat4 V = inverse(C);
        mat4 PV = P * V;

        fps_camera_move(&ferret);

        { // world

            glDisable(GL_DEPTH_TEST);
            {
                int3 triangle_indices[] = {{0, 1, 2}, {0, 2, 3}};
                vec2 vertex_texCoords[] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
                mat4 M = C;

                vec3 back_vertex_positions[] = {
                    {-1, -1, -1},
                    {1, -1, -1},
                    {1, 1, -1},
                    {-1, 1, -1},
                };
                fancy_draw(P, V, M,
                           2, triangle_indices, 4, back_vertex_positions,
                           NULL, NULL, {},
                           vertex_texCoords, "back.png");

                vec3 right_vertex_positions[] = {
                    {1, -1, -1},
                    {1, -1, 1},
                    {1, 1, 1},
                    {1, 1, -1},
                };
                fancy_draw(P, V, M,
                           2, triangle_indices, 4, right_vertex_positions,
                           NULL, NULL, {},
                           vertex_texCoords, "right.png");

                vec3 front_vertex_positions[] = {
                    {1, -1, 1},
                    {-1, -1, 1},
                    {-1, 1, 1},
                    {1, 1, 1},
                };
                fancy_draw(P, V, M,
                           2, triangle_indices, 4, front_vertex_positions,
                           NULL, NULL, {},
                           vertex_texCoords, "front.png");

                vec3 left_vertex_positions[] = {
                    {-1, -1, 1},
                    {-1, -1, -1},
                    {-1, 1, -1},
                    {-1, 1, 1},
                };
                fancy_draw(P, V, M,
                           2, triangle_indices, 4, left_vertex_positions,
                           NULL, NULL, {},
                           vertex_texCoords, "left.png");

                vec3 top_vertex_positions[] = {
                    {1, 1, 1},
                    {-1, 1, 1},
                    {-1, 1, -1},
                    {1, 1, -1},
                };
                fancy_draw(P, V, M,
                           2, triangle_indices, 4, top_vertex_positions,
                           NULL, NULL, {},
                           vertex_texCoords, "up.png");

                vec3 bottom_vertex_positions[] = {
                    {-1, -1, 1},
                    {1, -1, 1},
                    {1, -1, -1},
                    {-1, -1, -1},
                };
                fancy_draw(P, V, M,
                           2, triangle_indices, 4, bottom_vertex_positions,
                           NULL, NULL, {},
                           vertex_texCoords, "down.png");
            } glEnable(GL_DEPTH_TEST);

            { 
                vec3 verts[] = {{-box, 0, -box}, {-box, 0, box}, {box, 0, box}, {box, 0, -box}};
                vec3 colors[] = {monokai.white, AVG(monokai.white, monokai.gray), AVG(monokai.white, monokai.blue), AVG(monokai.white, monokai.purple)};
                basic_draw(QUADS, PV, 4, verts, colors);
            }
        }

        if (imgui_button("reset", 'r'))
        {
            init_balls();
        }

        for (int ball_i = 0; ball_i < num_balls; ball_i++)
        {
            Ball *cur_ball = &balls[ball_i];
            double ball_i_x = cur_ball->position.x;
            double ball_i_y = cur_ball->position.y;
            double ball_i_z = cur_ball->position.z;

            if (playing)
            {
                if (cur_ball->position.y > size)
                {
                    cur_ball->velocity.y += mass * g * .01;
                }
                else
                {
                    cur_ball->velocity.y *= -.8;
                }

                if (!IN_RANGE(cur_ball->position.x, -box, box))
                {
                    cur_ball->velocity.x *= -1;
                }
                if (!IN_RANGE(cur_ball->position.z, -box, box))
                {
                    cur_ball->velocity.z *= -1;
                }

                bool ferret_collide = sqrt(pow(ferret.origin.x - ball_i_x, 2) + pow(ferret.origin.y - ball_i_y, 2) + pow(ferret.origin.z - ball_i_z, 2)) <= size;
                if (ferret_collide)
                {
                    cur_ball->velocity += -1.5 * normalized(ferret.origin - cur_ball->position);
                }

                cur_ball->velocity.x *= .9;
                cur_ball->velocity.z *= .9;

                cur_ball->position.x += cur_ball->velocity.x;
                cur_ball->position.y = CLAMP(cur_ball->position.y + cur_ball->velocity.y, size - .1, 100);
                cur_ball->position.z += cur_ball->velocity.z;

                for (int ball_j = 0; ball_j < num_balls; ball_j++)
                {
                    Ball *test_ball = &balls[ball_j];
                    if (ball_j != ball_i)
                    {
                        double ball_j_x = test_ball->position.y;
                        double ball_j_y = test_ball->position.y;
                        double ball_j_z = test_ball->position.z;

                        bool intersect = sqrt(pow(ball_i_x - ball_j_x, 2) + pow(ball_i_y - ball_j_y, 2) + pow(ball_i_z - ball_j_z, 2)) <= 2 * size;

                        if (intersect)
                        {
                            vec3 temp_vec = cur_ball->velocity;
                            cur_ball->velocity = test_ball->velocity;
                            test_ball->velocity = temp_vec;
                        }
                    }
                }
            }
            fancy_draw(P, V, Translation(cur_ball->position.x, cur_ball->position.y, cur_ball->position.z) * Scaling(size, size, size), meshlib.fancy_sphere, cur_ball->color);
        }
    }
}

void hw()
{
    final();
}

int main()
{
    init();
    hw();
    return 0;
}
